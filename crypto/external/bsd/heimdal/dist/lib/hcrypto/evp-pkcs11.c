/*	$NetBSD: evp-pkcs11.c,v 1.2 2017/01/28 21:31:47 christos Exp $	*/

/*
 * Copyright (c) 2015-2016, Secure Endpoints Inc.
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 *
 * - Redistributions of source code must retain the above copyright
 *   notice, this list of conditions and the following disclaimer.
 *
 * - Redistributions in binary form must reproduce the above copyright
 *   notice, this list of conditions and the following disclaimer in
 *   the documentation and/or other materials provided with the
 *   distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS
 * FOR A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE
 * COPYRIGHT HOLDER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT,
 * INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
 * (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR
 * SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT,
 * STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED
 * OF THE POSSIBILITY OF SUCH DAMAGE.
 */

/* PKCS#11 provider */

#include <config.h>
#include <krb5/roken.h>
#include <assert.h>
#ifdef HAVE_DLFCN_H
#include <dlfcn.h>
#ifndef RTLD_LAZY
#define RTLD_LAZY 0
#endif
#ifndef RTLD_LOCAL
#define RTLD_LOCAL 0
#endif
#ifndef RTLD_GROUP
#define RTLD_GROUP 0
#endif
#ifndef RTLD_NODELETE
#define RTLD_NODELETE 0
#endif
#else
#error PKCS11 support requires dlfcn.h
#endif

#include <krb5/heimbase.h>

#include <evp.h>
#include <evp-hcrypto.h>
#include <evp-pkcs11.h>

#include <ref/pkcs11.h>

#if __sun && !defined(PKCS11_MODULE_PATH)
# if _LP64
# define PKCS11_MODULE_PATH "/usr/lib/64/libpkcs11.so"
# else
# define PKCS11_MODULE_PATH "/usr/lib/libpkcs11.so"
# endif
#elif defined(__linux__)
/*
 * XXX We should have an autoconf check for OpenCryptoki and such
 * things.  However, there's no AC_CHECK_OBJECT(), and we'd have to
 * write one.  Today I'm feeling lazy.  Another possibility would be to
 * have a symlink from the libdir we'll install into, and then we could
 * dlopen() that on all platforms.
 *
 * XXX Also, we should pick an appropriate shared object based on 32- vs
 * 64-bits.
 */
# define PKCS11_MODULE_PATH "/usr/lib/pkcs11/PKCS11_API.so"
#endif

static CK_FUNCTION_LIST_PTR p11_module;

static int
p11_cleanup(EVP_CIPHER_CTX *ctx);

struct pkcs11_cipher_ctx {
    CK_SESSION_HANDLE hSession;
    CK_OBJECT_HANDLE hSecret;
    int cipher_init_done;
};

struct pkcs11_md_ctx {
    CK_SESSION_HANDLE hSession;
};

static void *pkcs11_module_handle;
static void
p11_module_init_once(void *context)
{
    CK_RV rv;
    CK_FUNCTION_LIST_PTR module;
    CK_RV (*C_GetFunctionList_fn)(CK_FUNCTION_LIST_PTR_PTR);

    if (!issuid()) {
        char *pkcs11ModulePath = getenv("PKCS11_MODULE_PATH");
        if (pkcs11ModulePath != NULL) {
	    pkcs11_module_handle =
		dlopen(pkcs11ModulePath,
		       RTLD_LAZY | RTLD_LOCAL | RTLD_GROUP | RTLD_NODELETE);
	    if (pkcs11_module_handle == NULL)
                fprintf(stderr, "p11_module_init(%s): %s\n", pkcs11ModulePath, dlerror());
        }
    }
#ifdef PKCS11_MODULE_PATH
    if (pkcs11_module_handle == NULL) {
	pkcs11_module_handle =
	    dlopen(PKCS11_MODULE_PATH,
		   RTLD_LAZY | RTLD_LOCAL | RTLD_GROUP | RTLD_NODELETE);
	if (pkcs11_module_handle == NULL)
            fprintf(stderr, "p11_module_init(%s): %s\n", PKCS11_MODULE_PATH, dlerror());
    }
#endif
    if (pkcs11_module_handle == NULL)
        goto cleanup;

    C_GetFunctionList_fn = (CK_RV (*)(CK_FUNCTION_LIST_PTR_PTR))
	dlsym(pkcs11_module_handle, "C_GetFunctionList");
    if (C_GetFunctionList_fn == NULL)
        goto cleanup;

    rv = C_GetFunctionList_fn(&module);
    if (rv != CKR_OK)
        goto cleanup;

    rv = module->C_Initialize(NULL);
    if (rv == CKR_CRYPTOKI_ALREADY_INITIALIZED)
        rv = CKR_OK;
    if (rv == CKR_OK)
        *((CK_FUNCTION_LIST_PTR_PTR)context) = module;

cleanup:
    if (pkcs11_module_handle != NULL && p11_module == NULL) {
	dlclose(pkcs11_module_handle);
	pkcs11_module_handle = NULL;
    }
    /* else leak pkcs11_module_handle */
}

static CK_RV
p11_module_init(void)
{
    static heim_base_once_t init_module = HEIM_BASE_ONCE_INIT;

    heim_base_once_f(&init_module, &p11_module, p11_module_init_once);

    return p11_module != NULL ? CKR_OK : CKR_LIBRARY_LOAD_FAILED;
}

static CK_RV
p11_session_init(CK_MECHANISM_TYPE mechanismType, CK_SESSION_HANDLE_PTR phSession)
{
    CK_RV rv;
    CK_ULONG i, ulSlotCount = 0;
    CK_SLOT_ID_PTR pSlotList = NULL;
    CK_MECHANISM_INFO info;

    if (phSession != NULL)
        *phSession = CK_INVALID_HANDLE;

    rv = p11_module_init();
    if (rv != CKR_OK)
        goto cleanup;

    assert(p11_module != NULL);

    rv = p11_module->C_GetSlotList(CK_FALSE, NULL, &ulSlotCount);
    if (rv != CKR_OK)
        goto cleanup;

    pSlotList = (CK_SLOT_ID_PTR)calloc(ulSlotCount, sizeof(CK_SLOT_ID));
    if (pSlotList == NULL) {
        rv = CKR_HOST_MEMORY;
        goto cleanup;
    }

    rv = p11_module->C_GetSlotList(CK_FALSE, pSlotList, &ulSlotCount);
    if (rv != CKR_OK)
        goto cleanup;

    /*
     * Note that this approach of using the first slot that supports the desired
     * mechanism may not always be what the user wants (for example it may prefer
     * software to hardware crypto). We're going to assume that this code will be
     * principally used on Solaris (which has a meta-slot provider that sorts by
     * hardware first) or in situations where the user can configure the slots in
     * order of provider preference. In the future we should make this configurable.
     */
    for (i = 0; i < ulSlotCount; i++) {
        rv = p11_module->C_GetMechanismInfo(pSlotList[i], mechanismType, &info);
        if (rv == CKR_OK)
            break;
    }

    if (i == ulSlotCount) {
        rv = CKR_MECHANISM_INVALID;
        goto cleanup;
    }

    if (phSession != NULL) {
        rv = p11_module->C_OpenSession(pSlotList[i], CKF_SERIAL_SESSION, NULL, NULL, phSession);
        if (rv != CKR_OK)
            goto cleanup;
    }

cleanup:
    free(pSlotList);

    return rv;
}

static int
p11_mech_available_p(CK_MECHANISM_TYPE mechanismType)
{
    return p11_session_init(mechanismType, NULL) == CKR_OK;
}

static CK_KEY_TYPE
p11_key_type_for_mech(CK_MECHANISM_TYPE mechanismType)
{
    CK_KEY_TYPE keyType = 0;

    switch (mechanismType) {
    case CKM_RC2_CBC:
        keyType = CKK_RC2;
        break;
    case CKM_RC4:
        keyType = CKK_RC4;
        break;
    case CKM_DES_CBC:
        keyType = CKK_DES;
        break;
    case CKM_DES3_CBC:
        keyType = CKK_DES3;
        break;
    case CKM_AES_CBC:
    case CKM_AES_CFB8:
        keyType = CKK_AES;
        break;
    case CKM_CAMELLIA_CBC:
        keyType = CKK_CAMELLIA;
        break;
    default:
        assert(0 && "Unknown PKCS#11 mechanism type");
        break;
    }

    return keyType;
}

static int
p11_key_init(EVP_CIPHER_CTX *ctx,
             const unsigned char *key,
             const unsigned char *iv,
             int encp)
{
    CK_RV rv;
    CK_BBOOL bFalse = CK_FALSE;
    CK_BBOOL bTrue = CK_TRUE;
    CK_MECHANISM_TYPE mechanismType = (CK_MECHANISM_TYPE)ctx->cipher->app_data;
    CK_KEY_TYPE keyType = p11_key_type_for_mech(mechanismType);
    CK_OBJECT_CLASS objectClass = CKO_SECRET_KEY;
    CK_ATTRIBUTE_TYPE op = encp ? CKA_ENCRYPT : CKA_DECRYPT;
    CK_ATTRIBUTE attributes[] = {
        { CKA_EXTRACTABLE,      &bFalse,        sizeof(bFalse)          },
        { CKA_CLASS,            &objectClass,   sizeof(objectClass)     },
        { CKA_KEY_TYPE,         &keyType,       sizeof(keyType)         },
        { CKA_TOKEN,            &bFalse,        sizeof(bFalse)          },
        { CKA_PRIVATE,          &bFalse,        sizeof(bFalse)          },
        { CKA_SENSITIVE,        &bTrue,         sizeof(bTrue)           },
        { CKA_VALUE,            (void *)key,    ctx->key_len            },
        { op,                   &bTrue,         sizeof(bTrue)           }
    };
    struct pkcs11_cipher_ctx *p11ctx = (struct pkcs11_cipher_ctx *)ctx->cipher_data;
    p11ctx->cipher_init_done = 0;

    rv = p11_session_init(mechanismType, &p11ctx->hSession);
    if (rv != CKR_OK)
        goto cleanup;

    assert(p11_module != NULL);

    rv = p11_module->C_CreateObject(p11ctx->hSession, attributes,
                                    sizeof(attributes) / sizeof(attributes[0]),
                                    &p11ctx->hSecret);
    if (rv != CKR_OK)
        goto cleanup;

cleanup:
    if (rv != CKR_OK)
        p11_cleanup(ctx);

    return rv == CKR_OK;
}

static int
p11_do_cipher(EVP_CIPHER_CTX *ctx,
              unsigned char *out,
              const unsigned char *in,
              unsigned int size)
{
    struct pkcs11_cipher_ctx *p11ctx = (struct pkcs11_cipher_ctx *)ctx->cipher_data;
    CK_RV rv = CKR_OK;
    CK_ULONG ulCipherTextLen = size;
    CK_MECHANISM_TYPE mechanismType = (CK_MECHANISM_TYPE)ctx->cipher->app_data;
    CK_MECHANISM mechanism = {
        mechanismType,
        ctx->cipher->iv_len ? ctx->iv : NULL,
        ctx->cipher->iv_len
    };

    assert(p11_module != NULL);
    /* The EVP layer only ever calls us with complete cipher blocks */
    assert(EVP_CIPHER_CTX_mode(ctx) == EVP_CIPH_STREAM_CIPHER ||
           (size % ctx->cipher->block_size) == 0);

    if (ctx->encrypt) {
        if (!p11ctx->cipher_init_done) {
            rv = p11_module->C_EncryptInit(p11ctx->hSession, &mechanism, p11ctx->hSecret);
            if (rv == CKR_OK)
                p11ctx->cipher_init_done = 1;
        }
        if (rv == CKR_OK)
            rv = p11_module->C_EncryptUpdate(p11ctx->hSession, (unsigned char *)in, size, out, &ulCipherTextLen);
    } else {
        if (!p11ctx->cipher_init_done) {
            rv = p11_module->C_DecryptInit(p11ctx->hSession, &mechanism, p11ctx->hSecret);
            if (rv == CKR_OK)
                p11ctx->cipher_init_done = 1;
        }
        if (rv == CKR_OK)
            rv = p11_module->C_DecryptUpdate(p11ctx->hSession, (unsigned char *)in, size, out, &ulCipherTextLen);
    }

    return rv == CKR_OK;
}

static int
p11_cleanup(EVP_CIPHER_CTX *ctx)
{
    struct pkcs11_cipher_ctx *p11ctx = (struct pkcs11_cipher_ctx *)ctx->cipher_data;

    assert(p11_module != NULL);

    if (p11ctx->hSecret != CK_INVALID_HANDLE)  {
        p11_module->C_DestroyObject(p11ctx->hSession, p11ctx->hSecret);
        p11ctx->hSecret = CK_INVALID_HANDLE;
    }
    if (p11ctx->hSession != CK_INVALID_HANDLE) {
        p11_module->C_CloseSession(p11ctx->hSession);
        p11ctx->hSession = CK_INVALID_HANDLE;
    }

    return 1;
}

static int
p11_md_hash_init(CK_MECHANISM_TYPE mechanismType, EVP_MD_CTX *ctx)
{
    struct pkcs11_md_ctx *p11ctx = (struct pkcs11_md_ctx *)ctx;
    CK_RV rv;

    rv = p11_session_init(mechanismType, &p11ctx->hSession);
    if (rv == CKR_OK) {
        CK_MECHANISM mechanism = { mechanismType, NULL, 0 };

        assert(p11_module != NULL);

        rv = p11_module->C_DigestInit(p11ctx->hSession, &mechanism);
    }

    return rv == CKR_OK;
}

static int
p11_md_update(EVP_MD_CTX *ctx, const void *data, size_t length)
{
    struct pkcs11_md_ctx *p11ctx = (struct pkcs11_md_ctx *)ctx;
    CK_RV rv;

    assert(p11_module != NULL);

    rv = p11_module->C_DigestUpdate(p11ctx->hSession, (unsigned char *)data, length);

    return rv == CKR_OK;
}

static int
p11_md_final(void *digest, EVP_MD_CTX *ctx)
{
    struct pkcs11_md_ctx *p11ctx = (struct pkcs11_md_ctx *)ctx;
    CK_RV rv;
    CK_ULONG digestLen = 0;

    assert(p11_module != NULL);

    rv = p11_module->C_DigestFinal(p11ctx->hSession, NULL, &digestLen);
    if (rv == CKR_OK)
        rv = p11_module->C_DigestFinal(p11ctx->hSession, digest, &digestLen);

    return rv == CKR_OK;
}

static int
p11_md_cleanup(EVP_MD_CTX *ctx)
{
    struct pkcs11_md_ctx *p11ctx = (struct pkcs11_md_ctx *)ctx;
    CK_RV rv;

    assert(p11_module != NULL);

    rv = p11_module->C_CloseSession(p11ctx->hSession);
    if (rv == CKR_OK)
        p11ctx->hSession = CK_INVALID_HANDLE;

    return rv == CKR_OK;
}

#define PKCS11_CIPHER_ALGORITHM(name, mechanismType, block_size,        \
                                key_len, iv_len, flags)                 \
                                                                        \
    static EVP_CIPHER                                                   \
    pkcs11_##name = {                                                   \
        0,                                                              \
        block_size,                                                     \
        key_len,                                                        \
        iv_len,                                                         \
        flags,                                                          \
        p11_key_init,                                                   \
        p11_do_cipher,                                                  \
        p11_cleanup,                                                    \
        sizeof(struct pkcs11_cipher_ctx),                               \
        NULL,                                                           \
        NULL,                                                           \
        NULL,                                                           \
        (void *)mechanismType                                           \
    };                                                                  \
                                                                        \
    const EVP_CIPHER *                                                  \
    hc_EVP_pkcs11_##name(void)                                          \
    {                                                                   \
        if (p11_mech_available_p(mechanismType))                        \
            return &pkcs11_##name;                                      \
        else                                                            \
            return NULL;                                                \
    }                                                                   \
                                                                        \
    static void                                                         \
    pkcs11_hcrypto_##name##_init_once(void *context)                    \
    {                                                                   \
        const EVP_CIPHER *cipher;                                       \
                                                                        \
        cipher = hc_EVP_pkcs11_ ##name();                               \
        if (cipher == NULL && HCRYPTO_FALLBACK)                         \
            cipher = hc_EVP_hcrypto_ ##name();                          \
                                                                        \
        *((const EVP_CIPHER **)context) = cipher;                       \
    }                                                                   \
                                                                        \
    const EVP_CIPHER *                                                  \
    hc_EVP_pkcs11_hcrypto_##name(void)                                  \
    {                                                                   \
        static const EVP_CIPHER *__cipher;                              \
        static heim_base_once_t __init = HEIM_BASE_ONCE_INIT;           \
                                                                        \
        heim_base_once_f(&__init, &__cipher,                            \
                         pkcs11_hcrypto_##name##_init_once);            \
                                                                        \
        return __cipher;                                                \
    }

#define PKCS11_MD_ALGORITHM(name, mechanismType, hash_size, block_size) \
                                                                        \
    static int p11_##name##_init(EVP_MD_CTX *ctx)                       \
    {                                                                   \
        return p11_md_hash_init(mechanismType, ctx);                    \
    }                                                                   \
                                                                        \
    const EVP_MD *                                                      \
    hc_EVP_pkcs11_##name(void)                                          \
    {                                                                   \
        static struct hc_evp_md name = {                                \
            hash_size,                                                  \
            block_size,                                                 \
            sizeof(struct pkcs11_md_ctx),                               \
            p11_##name##_init,                                          \
            p11_md_update,                                              \
            p11_md_final,                                               \
            p11_md_cleanup                                              \
        };                                                              \
                                                                        \
        if (p11_mech_available_p(mechanismType))                        \
            return &name;                                               \
        else                                                            \
            return NULL;                                                \
    }                                                                   \
                                                                        \
    static void                                                         \
    pkcs11_hcrypto_##name##_init_once(void *context)                    \
    {                                                                   \
        const EVP_MD *md;                                               \
                                                                        \
        md = hc_EVP_pkcs11_ ##name();                                   \
        if (md == NULL && HCRYPTO_FALLBACK)                             \
            md = hc_EVP_hcrypto_ ##name();                              \
                                                                        \
        *((const EVP_MD **)context) = md;                               \
    }                                                                   \
                                                                        \
    const EVP_MD *                                                      \
    hc_EVP_pkcs11_hcrypto_##name(void)                                  \
    {                                                                   \
        static const EVP_MD *__md;                                      \
        static heim_base_once_t __init = HEIM_BASE_ONCE_INIT;           \
                                                                        \
        heim_base_once_f(&__init, &__md,                                \
                         pkcs11_hcrypto_##name##_init_once);            \
                                                                        \
        return __md;                                                    \
    }

#define PKCS11_MD_ALGORITHM_UNAVAILABLE(name)                           \
                                                                        \
    const EVP_MD *                                                      \
    hc_EVP_pkcs11_##name(void)                                          \
    {                                                                   \
        return NULL;                                                    \
    }                                                                   \
                                                                        \
    const EVP_MD *                                                      \
    hc_EVP_pkcs11_hcrypto_##name(void)                                  \
    {                                                                   \
        return hc_EVP_hcrypto_ ##name();                                \
    }

/**
 * The triple DES cipher type (PKCS#11 provider)
 *
 * @return the DES-EDE3-CBC EVP_CIPHER pointer.
 *
 * @ingroup hcrypto_evp
 */

PKCS11_CIPHER_ALGORITHM(des_ede3_cbc,
                        CKM_DES3_CBC,
                        8,
                        24,
                        8,
                        EVP_CIPH_CBC_MODE)

/**
 * The DES cipher type (PKCS#11 provider)
 *
 * @return the DES-CBC EVP_CIPHER pointer.
 *
 * @ingroup hcrypto_evp
 */

PKCS11_CIPHER_ALGORITHM(des_cbc,
                        CKM_DES_CBC,
                        8,
                        8,
                        8,
                        EVP_CIPH_CBC_MODE)

/**
 * The AES-128 cipher type (PKCS#11 provider)
 *
 * @return the AES-128-CBC EVP_CIPHER pointer.
 *
 * @ingroup hcrypto_evp
 */

PKCS11_CIPHER_ALGORITHM(aes_128_cbc,
                        CKM_AES_CBC,
                        16,
                        16,
                        16,
                        EVP_CIPH_CBC_MODE)

/**
 * The AES-192 cipher type (PKCS#11 provider)
 *
 * @return the AES-192-CBC EVP_CIPHER pointer.
 *
 * @ingroup hcrypto_evp
 */

PKCS11_CIPHER_ALGORITHM(aes_192_cbc,
                        CKM_AES_CBC,
                        16,
                        24,
                        16,
                        EVP_CIPH_CBC_MODE)

/**
 * The AES-256 cipher type (PKCS#11 provider)
 *
 * @return the AES-256-CBC EVP_CIPHER pointer.
 *
 * @ingroup hcrypto_evp
 */

PKCS11_CIPHER_ALGORITHM(aes_256_cbc,
                        CKM_AES_CBC,
                        16,
                        32,
                        16,
                        EVP_CIPH_CBC_MODE)

/**
 * The AES-128 CFB8 cipher type (PKCS#11 provider)
 *
 * @return the AES-128-CFB8 EVP_CIPHER pointer.
 *
 * @ingroup hcrypto_evp
 */

PKCS11_CIPHER_ALGORITHM(aes_128_cfb8,
                        CKM_AES_CFB8,
                        16,
                        16,
                        16,
                        EVP_CIPH_CFB8_MODE)

/**
 * The AES-192 CFB8 cipher type (PKCS#11 provider)
 *
 * @return the AES-192-CFB8 EVP_CIPHER pointer.
 *
 * @ingroup hcrypto_evp
 */

PKCS11_CIPHER_ALGORITHM(aes_192_cfb8,
                        CKM_AES_CFB8,
                        16,
                        24,
                        16,
                        EVP_CIPH_CFB8_MODE)

/**
 * The AES-256 CFB8 cipher type (PKCS#11 provider)
 *
 * @return the AES-256-CFB8 EVP_CIPHER pointer.
 *
 * @ingroup hcrypto_evp
 */

PKCS11_CIPHER_ALGORITHM(aes_256_cfb8,
                        CKM_AES_CFB8,
                        16,
                        32,
                        16,
                        EVP_CIPH_CFB8_MODE)

/**
 * The RC2 cipher type - PKCS#11
 *
 * @return the RC2 EVP_CIPHER pointer.
 *
 * @ingroup hcrypto_evp
 */

PKCS11_CIPHER_ALGORITHM(rc2_cbc,
                        CKM_RC2_CBC,
                        8,
                        16,
                        8,
                        EVP_CIPH_CBC_MODE | EVP_CIPH_VARIABLE_LENGTH)

/**
 * The RC2-40 cipher type - PKCS#11
 *
 * @return the RC2-40 EVP_CIPHER pointer.
 *
 * @ingroup hcrypto_evp
 */

PKCS11_CIPHER_ALGORITHM(rc2_40_cbc,
                        CKM_RC2_CBC,
                        8,
                        5,
                        8,
                        EVP_CIPH_CBC_MODE)

/**
 * The RC2-64 cipher type - PKCS#11
 *
 * @return the RC2-64 EVP_CIPHER pointer.
 *
 * @ingroup hcrypto_evp
 */

PKCS11_CIPHER_ALGORITHM(rc2_64_cbc,
                        CKM_RC2_CBC,
                        8,
                        8,
                        8,
                        EVP_CIPH_CBC_MODE)

/**
 * The Camellia-128 cipher type - PKCS#11
 *
 * @return the Camellia-128 EVP_CIPHER pointer.
 *
 * @ingroup hcrypto_evp
 */

PKCS11_CIPHER_ALGORITHM(camellia_128_cbc,
                        CKM_CAMELLIA_CBC,
                        16,
                        16,
                        16,
                        EVP_CIPH_CBC_MODE)

/**
 * The Camellia-198 cipher type - PKCS#11
 *
 * @return the Camellia-198 EVP_CIPHER pointer.
 *
 * @ingroup hcrypto_evp
 */

PKCS11_CIPHER_ALGORITHM(camellia_192_cbc,
                        CKM_CAMELLIA_CBC,
                        16,
                        24,
                        16,
                        EVP_CIPH_CBC_MODE)

/**
 * The Camellia-256 cipher type - PKCS#11
 *
 * @return the Camellia-256 EVP_CIPHER pointer.
 *
 * @ingroup hcrypto_evp
 */

PKCS11_CIPHER_ALGORITHM(camellia_256_cbc,
                        CKM_CAMELLIA_CBC,
                        16,
                        32,
                        16,
                        EVP_CIPH_CBC_MODE)

/**
 * The RC4 cipher type (PKCS#11 provider)
 *
 * @return the RC4 EVP_CIPHER pointer.
 *
 * @ingroup hcrypto_evp
 */

PKCS11_CIPHER_ALGORITHM(rc4,
                        CKM_RC4,
                        1,
                        16,
                        0,
                        EVP_CIPH_STREAM_CIPHER | EVP_CIPH_VARIABLE_LENGTH)

/**
 * The RC4-40 cipher type (PKCS#11 provider)
 *
 * @return the RC4 EVP_CIPHER pointer.
 *
 * @ingroup hcrypto_evp
 */

PKCS11_CIPHER_ALGORITHM(rc4_40,
                        CKM_RC4,
                        1,
                        5,
                        0,
                        EVP_CIPH_STREAM_CIPHER | EVP_CIPH_VARIABLE_LENGTH)

PKCS11_MD_ALGORITHM(md2,    CKM_MD2,    16, 16)
#ifdef CKM_MD4 /* non-standard extension */
PKCS11_MD_ALGORITHM(md4,    CKM_MD4,    16, 64)
#else
PKCS11_MD_ALGORITHM_UNAVAILABLE(md4)
#endif
PKCS11_MD_ALGORITHM(md5,    CKM_MD5,    16, 64)
PKCS11_MD_ALGORITHM(sha1,   CKM_SHA_1,  20, 64)
PKCS11_MD_ALGORITHM(sha256, CKM_SHA256, 32, 64)
PKCS11_MD_ALGORITHM(sha384, CKM_SHA384, 48, 128)
PKCS11_MD_ALGORITHM(sha512, CKM_SHA512, 64, 128)
