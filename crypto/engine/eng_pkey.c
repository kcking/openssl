/*
 * Copyright 2001-2021 The OpenSSL Project Authors. All Rights Reserved.
 *
 * Licensed under the Apache License 2.0 (the "License").  You may not use
 * this file except in compliance with the License.  You can obtain a copy
 * in the file LICENSE in the source distribution or at
 * https://www.openssl.org/source/license.html
 */

/* We need to use some engine deprecated APIs */
#define OPENSSL_SUPPRESS_DEPRECATED

#include "eng_local.h"

/* Basic get/set stuff */

int ENGINE_set_load_privkey_function(ENGINE *e,
                                     ENGINE_LOAD_KEY_PTR loadpriv_f)
{
    e->load_privkey = loadpriv_f;
    return 1;
}

int ENGINE_set_load_pubkey_function(ENGINE *e, ENGINE_LOAD_KEY_PTR loadpub_f)
{
    e->load_pubkey = loadpub_f;
    return 1;
}

int ENGINE_set_load_ssl_client_cert_function(ENGINE *e,
                                             ENGINE_SSL_CLIENT_CERT_PTR
                                             loadssl_f)
{
    e->load_ssl_client_cert = loadssl_f;
    return 1;
}

ENGINE_LOAD_KEY_PTR ENGINE_get_load_privkey_function(const ENGINE *e)
{
    return e->load_privkey;
}

ENGINE_LOAD_KEY_PTR ENGINE_get_load_pubkey_function(const ENGINE *e)
{
    return e->load_pubkey;
}

ENGINE_SSL_CLIENT_CERT_PTR ENGINE_get_ssl_client_cert_function(const ENGINE
                                                               *e)
{
    return e->load_ssl_client_cert;
}

/* API functions to load public/private keys */

EVP_PKEY *ENGINE_load_private_key(ENGINE *e, const char *key_id,
                                  UI_METHOD *ui_method, void *callback_data)
{
    EVP_PKEY *pkey;
    int copied_key;

    if (e == NULL) {
        ERR_raise(ERR_LIB_ENGINE, ERR_R_PASSED_NULL_PARAMETER);
        return NULL;
    }
    if (!CRYPTO_THREAD_write_lock(global_engine_lock))
        return NULL;
    if (e->funct_ref == 0) {
        CRYPTO_THREAD_unlock(global_engine_lock);
        ERR_raise(ERR_LIB_ENGINE, ENGINE_R_NOT_INITIALISED);
        return NULL;
    }
    CRYPTO_THREAD_unlock(global_engine_lock);
    if (!e->load_privkey) {
        ERR_raise(ERR_LIB_ENGINE, ENGINE_R_NO_LOAD_FUNCTION);
        return NULL;
    }
    pkey = e->load_privkey(e, key_id, ui_method, callback_data);
    if (pkey == NULL) {
        ERR_raise(ERR_LIB_ENGINE, ENGINE_R_FAILED_LOADING_PRIVATE_KEY);
        return NULL;
    }
    /* We enforce check for legacy key */
    copied_key = 0;
    switch (EVP_PKEY_get_id(pkey)) {
    case EVP_PKEY_RSA:
        {
        RSA *rsa = EVP_PKEY_get1_RSA(pkey);
        EVP_PKEY_set1_RSA(pkey, rsa);
        copied_key = 1;
        RSA_free(rsa);
        }
        break;
#  ifndef OPENSSL_NO_EC
    case EVP_PKEY_SM2:
    case EVP_PKEY_EC:
        {
        EC_KEY *ec = EVP_PKEY_get1_EC_KEY(pkey);
        EVP_PKEY_set1_EC_KEY(pkey, ec);
        copied_key = 1;
        EC_KEY_free(ec);
        }
        break;
#  endif
#  ifndef OPENSSL_NO_DSA
    case EVP_PKEY_DSA:
        {
        DSA *dsa = EVP_PKEY_get1_DSA(pkey);
        EVP_PKEY_set1_DSA(pkey, dsa);
        copied_key = 1;
        DSA_free(dsa);
        }
        break;
#endif
#  ifndef OPENSSL_NO_DH
    case EVP_PKEY_DH:
        {
        DH *dh = EVP_PKEY_get1_DH(pkey);
        EVP_PKEY_set1_DH(pkey, dh);
        copied_key = 1;
        DH_free(dh);
        }
        break;
#endif
    default:
        /*Do nothing */
        break;
    }

    /* Preserve pkey->pmeth_engine so that custom methods set on this key by an
     * engine are preserved (for example in the libp11 pkcs11 engine). */
    if (copied_key) {
        EVP_PKEY_set1_engine(pkey, e);
    }

    return pkey;
}

EVP_PKEY *ENGINE_load_public_key(ENGINE *e, const char *key_id,
                                 UI_METHOD *ui_method, void *callback_data)
{
    EVP_PKEY *pkey;

    if (e == NULL) {
        ERR_raise(ERR_LIB_ENGINE, ERR_R_PASSED_NULL_PARAMETER);
        return NULL;
    }
    if (!CRYPTO_THREAD_write_lock(global_engine_lock))
        return NULL;
    if (e->funct_ref == 0) {
        CRYPTO_THREAD_unlock(global_engine_lock);
        ERR_raise(ERR_LIB_ENGINE, ENGINE_R_NOT_INITIALISED);
        return NULL;
    }
    CRYPTO_THREAD_unlock(global_engine_lock);
    if (!e->load_pubkey) {
        ERR_raise(ERR_LIB_ENGINE, ENGINE_R_NO_LOAD_FUNCTION);
        return NULL;
    }
    pkey = e->load_pubkey(e, key_id, ui_method, callback_data);
    if (pkey == NULL) {
        ERR_raise(ERR_LIB_ENGINE, ENGINE_R_FAILED_LOADING_PUBLIC_KEY);
        return NULL;
    }
    return pkey;
}

int ENGINE_load_ssl_client_cert(ENGINE *e, SSL *s,
                                STACK_OF(X509_NAME) *ca_dn, X509 **pcert,
                                EVP_PKEY **ppkey, STACK_OF(X509) **pother,
                                UI_METHOD *ui_method, void *callback_data)
{

    if (e == NULL) {
        ERR_raise(ERR_LIB_ENGINE, ERR_R_PASSED_NULL_PARAMETER);
        return 0;
    }
    if (!CRYPTO_THREAD_write_lock(global_engine_lock))
        return 0;
    if (e->funct_ref == 0) {
        CRYPTO_THREAD_unlock(global_engine_lock);
        ERR_raise(ERR_LIB_ENGINE, ENGINE_R_NOT_INITIALISED);
        return 0;
    }
    CRYPTO_THREAD_unlock(global_engine_lock);
    if (!e->load_ssl_client_cert) {
        ERR_raise(ERR_LIB_ENGINE, ENGINE_R_NO_LOAD_FUNCTION);
        return 0;
    }
    return e->load_ssl_client_cert(e, s, ca_dn, pcert, ppkey, pother,
                                   ui_method, callback_data);
}
