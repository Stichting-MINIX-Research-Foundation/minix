/*	$NetBSD: evp-openssl.c,v 1.2 2017/01/28 21:31:47 christos Exp $	*/

/*
 * Copyright (c) 2016, Kungliga Tekniska Högskolan
 * (Royal Institute of Technology, Stockholm, Sweden).
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

/* OpenSSL provider */

#include "config.h"
#include <krb5/roken.h>
#include <krb5/heimbase.h>

#include <assert.h>
#include <evp.h>

#ifdef HAVE_HCRYPTO_W_OPENSSL

/*
 * This is the OpenSSL 1.x backend for hcrypto.  It has been tested with
 * OpenSSL 1.0.1f and OpenSSL 1.1.0-pre3-dev.
 *
 * NOTE: In order for this to work with OpenSSL 1.1.x and up, it is
 *       critical to use opaque OpenSSL type accessors everywhere /
 *       never use knowledge of opaque OpenSSL type internals.
 */

#include <evp-openssl.h>

/*
 * This being an OpenSSL backend for hcrypto... we need to be able to
 * refer to types and objects (functions) from both, OpenSSL and
 * hcrypto.
 *
 * The hcrypto API is *very* similar to the OpenSSL 1.0.x API, with the
 * same type and symbol names in many cases, except that the hcrypto
 * names are prefixed with hc_*.  hcrypto has convenience macros that
 * provide OpenSSL aliases for the hcrypto interfaces, and hcrypto
 * applications are expected to use the OpenSSL names.
 *
 * Since here we must be able to refer to types and objects from both
 * OpenSSL and from hcrypto, we disable the hcrypto renaming for the
 * rest of this file.  These #undefs could be collected into an
 * <hcrypto/undef.h> for the purpose of permitting other applications to
 * use both, hcrypto and OpenSSL in the same source files (provided that
 * such applications refer to hcrypto types and objects by their proper
 * hc_-prefixed names).
 */
#include <undef.h>

/* Now it's safe to include OpenSSL headers */
#include <openssl/evp.h>

#if OPENSSL_VERSION_NUMBER < 0x10100000L || defined(LIBRESSL_VERSION_NUMBER)
#define EVP_MD_CTX_new EVP_MD_CTX_create
#define EVP_MD_CTX_free EVP_MD_CTX_destroy
#endif

/* A HEIM_BASE_ONCE argument struct for per-EVP one-time initialization */
struct once_init_cipher_ctx {
    const hc_EVP_CIPHER **hc_memoizep;
    hc_EVP_CIPHER *hc_memoize;
    const hc_EVP_CIPHER *fallback;
    unsigned long flags;
    int nid;
};

/* Our wrapper for OpenSSL EVP_CIPHER_CTXs */
struct ossl_cipher_ctx {
    EVP_CIPHER_CTX      *ossl_cipher_ctx;   /* OpenSSL cipher ctx */
    const EVP_CIPHER    *ossl_cipher;       /* OpenSSL cipher */
    int                 initialized;
};

/*
 * Our hc_EVP_CIPHER init() method; wraps around OpenSSL
 * EVP_CipherInit_ex().
 *
 * This is very similar to the init() function pointer in an OpenSSL
 * EVP_CIPHER, but a) we can't access them in 1.1, and b) the method
 * invocation protocols in hcrypto and OpenSSL are similar but not the
 * same, thus we must have this wrapper.
 */
static int
cipher_ctx_init(hc_EVP_CIPHER_CTX *ctx, const unsigned char *key,
                const unsigned char *iv, int enc)
{
    struct ossl_cipher_ctx *ossl_ctx = ctx->cipher_data; /* EVP_CIPHER_CTX wrapper */
    const EVP_CIPHER *c;

    assert(ossl_ctx != NULL);
    assert(ctx->cipher != NULL);
    assert(ctx->cipher->app_data != NULL);

    /*
     * Here be dragons.
     *
     * We need to make sure that the OpenSSL EVP_CipherInit_ex() is
     * called with cipher!=NULL just once per EVP_CIPHER_CTX, otherwise
     * state in the OpenSSL EVP_CIPHER_CTX will get cleaned up and then
     * we'll segfault.
     *
     * hcrypto applications can re-initialize an (hc_)EVP_CIPHER_CTX as
     * usual by calling (hc)EVP_CipherInit_ex() with a non-NULL cipher
     * argument, and that will cause cipher_cleanup() (below) to be
     * called.
     */
    c = ossl_ctx->ossl_cipher = ctx->cipher->app_data; /* OpenSSL's EVP_CIPHER * */
    if (!ossl_ctx->initialized) {
        ossl_ctx->ossl_cipher_ctx = EVP_CIPHER_CTX_new();
        if (ossl_ctx->ossl_cipher_ctx == NULL)
            return 0;
        /*
         * So we always call EVP_CipherInit_ex() with c!=NULL, but other
         * things NULL...
         */
        if (!EVP_CipherInit_ex(ossl_ctx->ossl_cipher_ctx, c, NULL, NULL, NULL, enc))
            return 0;
        ossl_ctx->initialized = 1;
    }

    /* ...and from here on always call EVP_CipherInit_ex() with c=NULL */
    if ((ctx->cipher->flags & hc_EVP_CIPH_VARIABLE_LENGTH) &&
        ctx->key_len > 0)
        EVP_CIPHER_CTX_set_key_length(ossl_ctx->ossl_cipher_ctx, ctx->key_len);

    return EVP_CipherInit_ex(ossl_ctx->ossl_cipher_ctx, NULL, NULL, key, iv, enc);
}

static int
cipher_do_cipher(hc_EVP_CIPHER_CTX *ctx, unsigned char *out,
                 const unsigned char *in, unsigned int len)
{
    struct ossl_cipher_ctx *ossl_ctx = ctx->cipher_data;

    assert(ossl_ctx != NULL);
    return EVP_Cipher(ossl_ctx->ossl_cipher_ctx, out, in, len);
}

static int
cipher_cleanup(hc_EVP_CIPHER_CTX *ctx)
{
    struct ossl_cipher_ctx *ossl_ctx = ctx->cipher_data;

    if (ossl_ctx == NULL || !ossl_ctx->initialized)
        return 1;

    if (ossl_ctx->ossl_cipher_ctx != NULL)
        EVP_CIPHER_CTX_free(ossl_ctx->ossl_cipher_ctx);

    ossl_ctx->ossl_cipher_ctx = NULL;
    ossl_ctx->ossl_cipher = NULL;
    ossl_ctx->initialized = 0;
    return 1;
}

static int
cipher_ctrl(hc_EVP_CIPHER_CTX *ctx, int type, int arg, void *ptr)
{
    struct ossl_cipher_ctx *ossl_ctx = ctx->cipher_data;

    assert(ossl_ctx != NULL);
    return EVP_CIPHER_CTX_ctrl(ossl_ctx->ossl_cipher_ctx, type, arg, ptr);
}


static void
get_EVP_CIPHER_once_cb(void *d)
{
    struct once_init_cipher_ctx *arg = d;
    const EVP_CIPHER *ossl_evp;
    hc_EVP_CIPHER *hc_evp;

    hc_evp = arg->hc_memoize;

    /*
     * We lookup EVP_CIPHER *s by NID so that we don't fail to find a
     * symbol such as EVP_aes...() when libcrypto changes after build
     * time (e.g., updates, LD_LIBRARY_PATH/LD_PRELOAD).
     */
    ossl_evp = EVP_get_cipherbynid(arg->nid);
    if (ossl_evp == NULL) {
        (void) memset(hc_evp, 0, sizeof(*hc_evp));
#if HCRYPTO_FALLBACK
        *arg->hc_memoizep = arg->fallback;
#endif
        return;
    }

    /* Build the hc_EVP_CIPHER */
    hc_evp->nid = EVP_CIPHER_nid(ossl_evp); /* We would an hcrypto NIDs if we had them */
    hc_evp->block_size = EVP_CIPHER_block_size(ossl_evp);
    hc_evp->key_len = EVP_CIPHER_key_length(ossl_evp);
    hc_evp->iv_len = EVP_CIPHER_iv_length(ossl_evp);

    /*
     * We force hc_EVP_CipherInit_ex to always call our init() function,
     * otherwise we don't get a chance to call EVP_CipherInit_ex()
     * correctly.
     */
    hc_evp->flags = hc_EVP_CIPH_ALWAYS_CALL_INIT | arg->flags;

    /* Our cipher context */
    hc_evp->ctx_size = sizeof(struct ossl_cipher_ctx);

    /* Our wrappers */
    hc_evp->init = cipher_ctx_init;
    hc_evp->do_cipher = cipher_do_cipher;
    hc_evp->cleanup = cipher_cleanup;
    hc_evp->set_asn1_parameters = NULL;
    hc_evp->get_asn1_parameters = NULL;
    hc_evp->ctrl = cipher_ctrl;

    /* Our link to the OpenSSL EVP_CIPHER */
    hc_evp->app_data = (void *)ossl_evp;

    /* Finally, set the static hc_EVP_CIPHER * to the one we just built */
    *arg->hc_memoizep = hc_evp;
}

static const hc_EVP_CIPHER *
get_EVP_CIPHER(heim_base_once_t *once, hc_EVP_CIPHER *hc_memoize,
               const hc_EVP_CIPHER **hc_memoizep,
               const hc_EVP_CIPHER *fallback,
               unsigned long flags, int nid)
{
    struct once_init_cipher_ctx arg;

    arg.flags = flags;
    arg.hc_memoizep = hc_memoizep;
    arg.hc_memoize = hc_memoize;
    arg.fallback = fallback;
    arg.nid = nid;
    heim_base_once_f(once, &arg, get_EVP_CIPHER_once_cb);
    return *hc_memoizep; /* May be NULL */
}

#define OSSL_CIPHER_ALGORITHM(name, flags)                              \
    extern const hc_EVP_CIPHER *hc_EVP_hcrypto_##name(void);            \
    const hc_EVP_CIPHER *hc_EVP_ossl_##name(void)                       \
    {                                                                   \
        static hc_EVP_CIPHER ossl_##name##_st;                          \
        static const hc_EVP_CIPHER *ossl_##name;                        \
        static heim_base_once_t once = HEIM_BASE_ONCE_INIT;             \
        return get_EVP_CIPHER(&once, &ossl_##name##_st, &ossl_##name,   \
                              hc_EVP_hcrypto_##name(),                  \
                              flags, NID_##name);                       \
    }

/* As above, but for EVP_MDs */

struct ossl_md_ctx {
    EVP_MD_CTX          *ossl_md_ctx;       /* OpenSSL md ctx */
    const EVP_MD        *ossl_md;           /* OpenSSL md */
    int                 initialized;
};

static int
ossl_md_init(struct ossl_md_ctx *ctx, const EVP_MD *md)
{
    if (ctx->initialized)
        EVP_MD_CTX_free(ctx->ossl_md_ctx);
    ctx->initialized = 0;

    ctx->ossl_md = md;
    ctx->ossl_md_ctx = EVP_MD_CTX_new();
    if (!EVP_DigestInit(ctx->ossl_md_ctx, md)) {
        EVP_MD_CTX_free(ctx->ossl_md_ctx);
        ctx->ossl_md_ctx = NULL;
        ctx->ossl_md = NULL;
        return 0;
    }
    ctx->initialized = 1;
    return 1;
}

static int
ossl_md_update(hc_EVP_MD_CTX *d, const void *data, size_t count)
{
    struct ossl_md_ctx *ctx = (void *)d;

    return EVP_DigestUpdate(ctx->ossl_md_ctx, data, count);
}

static int
ossl_md_final(void *md_data, hc_EVP_MD_CTX *d)
{
    struct ossl_md_ctx *ctx = (void *)d;

    return EVP_DigestFinal(ctx->ossl_md_ctx, md_data, NULL);
}

static int
ossl_md_cleanup(hc_EVP_MD_CTX *d)
{
    struct ossl_md_ctx *ctx = (void *)d;

    if (!ctx->initialized)
        return 1;
    EVP_MD_CTX_free(ctx->ossl_md_ctx);
    ctx->ossl_md = NULL;
    ctx->initialized = 0;

    return 1;
}

struct once_init_md_ctx {
    const EVP_MD **ossl_memoizep;
    const hc_EVP_MD **hc_memoizep;
    hc_EVP_MD *hc_memoize;
    const hc_EVP_MD *fallback;
    hc_evp_md_init md_init;
    int nid;
};

static void
get_EVP_MD_once_cb(void *d)
{
    struct once_init_md_ctx *arg = d;
    const EVP_MD *ossl_evp;
    hc_EVP_MD *hc_evp;

    hc_evp = arg->hc_memoize;
    *arg->ossl_memoizep = ossl_evp = EVP_get_digestbynid(arg->nid);

    if (ossl_evp == NULL) {
        (void) memset(hc_evp, 0, sizeof(*hc_evp));
#if HCRYPTO_FALLBACK
        *arg->hc_memoizep = arg->fallback;
#endif
        return;
    }

    /* Build the hc_EVP_MD */
    hc_evp->block_size = EVP_MD_block_size(ossl_evp);
    hc_evp->hash_size = EVP_MD_size(ossl_evp);
    hc_evp->ctx_size = sizeof(struct ossl_md_ctx);
    hc_evp->init = arg->md_init;
    hc_evp->update = ossl_md_update;
    hc_evp->final = ossl_md_final;
    hc_evp->cleanup = ossl_md_cleanup;

    *arg->hc_memoizep = hc_evp;
}

static const hc_EVP_MD *
get_EVP_MD(heim_base_once_t *once, hc_EVP_MD *hc_memoize,
           const hc_EVP_MD **hc_memoizep, const EVP_MD **ossl_memoizep,
           const hc_EVP_MD *fallback,
           hc_evp_md_init md_init, int nid)
{
    struct once_init_md_ctx ctx;

    ctx.ossl_memoizep = ossl_memoizep;
    ctx.hc_memoizep = hc_memoizep;
    ctx.hc_memoize = hc_memoize;
    ctx.fallback = fallback;
    ctx.md_init = md_init;
    ctx.nid = nid;
    heim_base_once_f(once, &ctx, get_EVP_MD_once_cb);
    return *hc_memoizep; /* May be NULL */
}

#define OSSL_MD_ALGORITHM(name)                                         \
    extern const hc_EVP_MD *hc_EVP_hcrypto_##name(void);                \
    static const EVP_MD *ossl_EVP_##name;                               \
    static const hc_EVP_MD *ossl_##name;                                \
    static int ossl_init_##name(hc_EVP_MD_CTX *d)                       \
    {                                                                   \
        return ossl_md_init((void *)d, ossl_EVP_##name);                \
    }                                                                   \
    const hc_EVP_MD *hc_EVP_ossl_##name(void)                           \
    {                                                                   \
        static hc_EVP_MD ossl_##name##_st;                              \
        static heim_base_once_t once = HEIM_BASE_ONCE_INIT;             \
        return get_EVP_MD(&once, &ossl_##name##_st, &ossl_##name,       \
                          &ossl_EVP_##name, hc_EVP_hcrypto_##name(),    \
                          ossl_init_##name, NID_##name);                \
    }

#else /* HAVE_HCRYPTO_W_OPENSSL */

#include "evp-hcrypto.h"

#define OSSL_CIPHER_ALGORITHM(name, flags)                              \
    extern const hc_EVP_CIPHER *hc_EVP_ossl_##name(void);               \
    const hc_EVP_CIPHER *hc_EVP_ossl_##name(void)                       \
    {                                                                   \
        return hc_EVP_hcrypto_##name();                                 \
    }

#define OSSL_MD_ALGORITHM(name)                                         \
    extern const hc_EVP_MD *hc_EVP_ossl_##name(void);                   \
    const hc_EVP_MD *hc_EVP_ossl_##name(void)                           \
    {                                                                   \
        return hc_EVP_hcrypto_##name();                                 \
    }

#endif /* HAVE_HCRYPTO_W_OPENSSL */

/**
 * The triple DES cipher type (OpenSSL provider)
 *
 * @return the DES-EDE3-CBC EVP_CIPHER pointer.
 *
 * @ingroup hcrypto_evp
 */
OSSL_CIPHER_ALGORITHM(des_ede3_cbc, hc_EVP_CIPH_CBC_MODE)

/**
 * The DES cipher type (OpenSSL provider)
 *
 * @return the DES-CBC EVP_CIPHER pointer.
 *
 * @ingroup hcrypto_evp
 */
OSSL_CIPHER_ALGORITHM(des_cbc, hc_EVP_CIPH_CBC_MODE)

/**
 * The AES-128 cipher type (OpenSSL provider)
 *
 * @return the AES-128-CBC EVP_CIPHER pointer.
 *
 * @ingroup hcrypto_evp
 */
OSSL_CIPHER_ALGORITHM(aes_128_cbc, hc_EVP_CIPH_CBC_MODE)

/**
 * The AES-192 cipher type (OpenSSL provider)
 *
 * @return the AES-192-CBC EVP_CIPHER pointer.
 *
 * @ingroup hcrypto_evp
 */
OSSL_CIPHER_ALGORITHM(aes_192_cbc, hc_EVP_CIPH_CBC_MODE)

/**
 * The AES-256 cipher type (OpenSSL provider)
 *
 * @return the AES-256-CBC EVP_CIPHER pointer.
 *
 * @ingroup hcrypto_evp
 */
OSSL_CIPHER_ALGORITHM(aes_256_cbc, hc_EVP_CIPH_CBC_MODE)

/**
 * The AES-128 CFB8 cipher type (OpenSSL provider)
 *
 * @return the AES-128-CFB8 EVP_CIPHER pointer.
 *
 * @ingroup hcrypto_evp
 */
OSSL_CIPHER_ALGORITHM(aes_128_cfb8, hc_EVP_CIPH_CFB8_MODE)

/**
 * The AES-192 CFB8 cipher type (OpenSSL provider)
 *
 * @return the AES-192-CFB8 EVP_CIPHER pointer.
 *
 * @ingroup hcrypto_evp
 */
OSSL_CIPHER_ALGORITHM(aes_192_cfb8, hc_EVP_CIPH_CFB8_MODE)

/**
 * The AES-256 CFB8 cipher type (OpenSSL provider)
 *
 * @return the AES-256-CFB8 EVP_CIPHER pointer.
 *
 * @ingroup hcrypto_evp
 */
OSSL_CIPHER_ALGORITHM(aes_256_cfb8, hc_EVP_CIPH_CFB8_MODE)

/*
 * RC2 is only needed for tests of PKCS#12 support, which currently uses
 * the RC2 PBE.  So no RC2 -> tests fail.
 */

/**
 * The RC2 cipher type - OpenSSL
 *
 * @return the RC2 EVP_CIPHER pointer.
 *
 * @ingroup hcrypto_evp
 */
OSSL_CIPHER_ALGORITHM(rc2_cbc,
                      hc_EVP_CIPH_CBC_MODE |
                      hc_EVP_CIPH_VARIABLE_LENGTH)

/**
 * The RC2-40 cipher type - OpenSSL
 *
 * @return the RC2-40 EVP_CIPHER pointer.
 *
 * @ingroup hcrypto_evp
 */
OSSL_CIPHER_ALGORITHM(rc2_40_cbc,
                      hc_EVP_CIPH_CBC_MODE)

/**
 * The RC2-64 cipher type - OpenSSL
 *
 * @return the RC2-64 EVP_CIPHER pointer.
 *
 * @ingroup hcrypto_evp
 */
OSSL_CIPHER_ALGORITHM(rc2_64_cbc,
                      hc_EVP_CIPH_CBC_MODE |
                      hc_EVP_CIPH_VARIABLE_LENGTH)

/**
 * The Camellia-128 cipher type - OpenSSL
 *
 * @return the Camellia-128 EVP_CIPHER pointer.
 *
 * @ingroup hcrypto_evp
 */
OSSL_CIPHER_ALGORITHM(camellia_128_cbc, hc_EVP_CIPH_CBC_MODE)

/**
 * The Camellia-198 cipher type - OpenSSL
 *
 * @return the Camellia-198 EVP_CIPHER pointer.
 *
 * @ingroup hcrypto_evp
 */
OSSL_CIPHER_ALGORITHM(camellia_192_cbc, hc_EVP_CIPH_CBC_MODE)

/**
 * The Camellia-256 cipher type - OpenSSL
 *
 * @return the Camellia-256 EVP_CIPHER pointer.
 *
 * @ingroup hcrypto_evp
 */
OSSL_CIPHER_ALGORITHM(camellia_256_cbc, hc_EVP_CIPH_CBC_MODE)

/**
 * The RC4 cipher type (OpenSSL provider)
 *
 * @return the RC4 EVP_CIPHER pointer.
 *
 * @ingroup hcrypto_evp
 */
OSSL_CIPHER_ALGORITHM(rc4,
                      hc_EVP_CIPH_STREAM_CIPHER |
                      hc_EVP_CIPH_VARIABLE_LENGTH)

/**
 * The RC4-40 cipher type (OpenSSL provider)
 *
 * @return the RC4 EVP_CIPHER pointer.
 *
 * @ingroup hcrypto_evp
 */
OSSL_CIPHER_ALGORITHM(rc4_40,
                      hc_EVP_CIPH_STREAM_CIPHER |
                      hc_EVP_CIPH_VARIABLE_LENGTH)

/**
 * The MD2 hash algorithm (OpenSSL provider)
 *
 * @return the MD2 EVP_MD pointer.
 *
 * @ingroup hcrypto_evp
 */
OSSL_MD_ALGORITHM(md2)

/**
 * The MD4 hash algorithm (OpenSSL provider)
 *
 * @return the MD4 EVP_MD pointer.
 *
 * @ingroup hcrypto_evp
 */
OSSL_MD_ALGORITHM(md4)

/**
 * The MD5 hash algorithm (OpenSSL provider)
 *
 * @return the MD5 EVP_MD pointer.
 *
 * @ingroup hcrypto_evp
 */
OSSL_MD_ALGORITHM(md5)

/**
 * The SHA-1 hash algorithm (OpenSSL provider)
 *
 * @return the SHA-1 EVP_MD pointer.
 *
 * @ingroup hcrypto_evp
 */
OSSL_MD_ALGORITHM(sha1)

/**
 * The SHA-256 hash algorithm (OpenSSL provider)
 *
 * @return the SHA-256 EVP_MD pointer.
 *
 * @ingroup hcrypto_evp
 */
OSSL_MD_ALGORITHM(sha256)

/**
 * The SHA-384 hash algorithm (OpenSSL provider)
 *
 * @return the SHA-384 EVP_MD pointer.
 *
 * @ingroup hcrypto_evp
 */
OSSL_MD_ALGORITHM(sha384)

/**
 * The SHA-512 hash algorithm (OpenSSL provider)
 *
 * @return the SHA-512 EVP_MD pointer.
 *
 * @ingroup hcrypto_evp
 */
OSSL_MD_ALGORITHM(sha512)
