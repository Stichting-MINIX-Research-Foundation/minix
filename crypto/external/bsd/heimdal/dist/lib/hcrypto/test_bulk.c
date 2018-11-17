/*	$NetBSD: test_bulk.c,v 1.2 2017/01/28 21:31:47 christos Exp $	*/

/*
 * Copyright (c) 2006 Kungliga Tekniska Högskolan
 * (Royal Institute of Technology, Stockholm, Sweden).
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 *
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 *
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * 3. Neither the name of the Institute nor the names of its contributors
 *    may be used to endorse or promote products derived from this software
 *    without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE INSTITUTE AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE INSTITUTE OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 */

#include <config.h>
#include <krb5/roken.h>
#include <assert.h>
#include <krb5/getarg.h>

#include <evp.h>
#include <evp-hcrypto.h>
#include <evp-cc.h>
#if defined(_WIN32)
#include <evp-w32.h>
#endif
#include <evp-pkcs11.h>
#include <krb5/hex.h>
#include <err.h>

#ifdef WIN32
#define STATS_START(M)                                                      \
        LARGE_INTEGER StartingTime, EndingTime, ElapsedMicroseconds;        \
        LARGE_INTEGER Frequency;                                            \
                                                                            \
        QueryPerformanceFrequency(&Frequency);                              \
        QueryPerformanceCounter(&StartingTime);

#define STATS_END(M)                                                        \
        QueryPerformanceCounter(&EndingTime);                               \
        ElapsedMicroseconds.QuadPart = EndingTime.QuadPart - StartingTime.QuadPart; \
        ElapsedMicroseconds.QuadPart *= 1000000;                            \
        ElapsedMicroseconds.QuadPart /= Frequency.QuadPart;                 \
                                                                            \
        M += (ElapsedMicroseconds.QuadPart - M) / (i + 1);
#else
#define STATS_START(M)                                                      \
        struct timeval StartingTime, EndingTime;                            \
                                                                            \
        gettimeofday(&StartingTime, NULL);

#define STATS_END(M)                                                        \
        gettimeofday(&EndingTime, NULL);                                    \
        timevalsub(&EndingTime, &StartingTime);                             \
        M += (EndingTime.tv_sec * 1000000 + EndingTime.tv_usec - M) / (i + 1);
#endif

static int version_flag;
static int help_flag;
static int len = 1;
static int loops = 20;
static char *provider = "hcrypto";
static unsigned char *d;

#ifdef __APPLE__
#define PROVIDER_USAGE "hcrypto|cc"
#elif defined(WIN32)
#define PROVIDER_USAGE "hcrypto|w32crypto"
#elif __sun || defined(PKCS11_MODULE_PATH)
#define PROVIDER_USAGE "hcrypto|pkcs11"
#else
#define PROVIDER_USAGE "hcrypto"
#endif

static struct getargs args[] = {
    { "provider",	0,	arg_string,	&provider,
      "crypto provider", PROVIDER_USAGE },
    { "loops",		0,	arg_integer,	&loops,
      "number of loops", 	"loops" },
    { "size",	0,	arg_integer,	&len,
      "size (KB)", NULL },
    { "version",	0,	arg_flag,	&version_flag,
      "print version", NULL },
    { "help",		0,	arg_flag,	&help_flag,
      NULL, 	NULL }
};

static void
usage (int ret)
{
    arg_printusage (args,
		    sizeof(args)/sizeof(*args),
		    NULL,
		    "");
    exit (ret);
}

static int
test_bulk_cipher(const char *cname, const EVP_CIPHER *c)
{
    static unsigned char key[16];
    static unsigned char iv[16];
    int i;
    int64_t M = 0;

    if (c == NULL) {
        printf("%s not supported\n", cname);
	return 0;
    }

    for (i = 0; i < loops; i++) {
        EVP_CIPHER_CTX ectx;
        EVP_CIPHER_CTX dctx;

        STATS_START(M)

        EVP_CIPHER_CTX_init(&ectx);
        EVP_CIPHER_CTX_init(&dctx);

        if (EVP_CipherInit_ex(&ectx, c, NULL, NULL, NULL, 1) != 1)
	    errx(1, "can't init encrypt");
        if (EVP_CipherInit_ex(&dctx, c, NULL, NULL, NULL, 0) != 1)
	    errx(1, "can't init decrypt");

        EVP_CIPHER_CTX_set_key_length(&ectx, sizeof(key));
        EVP_CIPHER_CTX_set_key_length(&dctx, sizeof(key));

        if (EVP_CipherInit_ex(&ectx, NULL, NULL, key, iv, 1) != 1)
	    errx(1, "can't init encrypt");
        if (EVP_CipherInit_ex(&dctx, NULL, NULL, key, iv, 0) != 1)
	    errx(1, "can't init decrypt");

        if (!EVP_Cipher(&ectx, d, d, len))
	    errx(1, "can't encrypt");
        if (!EVP_Cipher(&dctx, d, d, len))
	    errx(1, "can't decrypt");

        EVP_CIPHER_CTX_cleanup(&ectx);
        EVP_CIPHER_CTX_cleanup(&dctx);

        STATS_END(M);

	if (d[0] != 0x00 || d[len - 1] != ((len - 1) & 0xff))
	    errx(1, "encrypt/decrypt inconsistent");
    }

    printf("%s: mean time %llu usec%s\n", cname, (unsigned long long)M,
           (M == 1) ? "" : "s");

    return 0;
}

static int
test_bulk_digest(const char *cname, const EVP_MD *md)
{
    char digest[EVP_MAX_MD_SIZE];
    int i;
    unsigned int tmp = sizeof(digest);
    int64_t M = 0;

    if (md == NULL) {
        printf("%s not supported\n", cname);
	return 0;
    }

    for (i = 0; i < loops; i++) {
        STATS_START(M);
        EVP_Digest(d, len, digest, &tmp, md, NULL);
        STATS_END(M);
    }

    printf("%s: mean time %llu usec%s\n", cname, (unsigned long long)M,
           (M == 1) ? "" : "s");

    return 0;
}

static void
test_bulk_provider_hcrypto(void)
{
    test_bulk_cipher("hcrypto_aes_256_cbc",	EVP_hcrypto_aes_256_cbc());
#if 0
    test_bulk_cipher("hcrypto_aes_256_cfb8",	EVP_hcrypto_aes_256_cfb8());
#endif
    test_bulk_cipher("hcrypto_rc4",		EVP_hcrypto_rc4());
    test_bulk_digest("hcrypto_md2",		EVP_hcrypto_md2());
    test_bulk_digest("hcrypto_md4",		EVP_hcrypto_md4());
    test_bulk_digest("hcrypto_md5",		EVP_hcrypto_md5());
    test_bulk_digest("hcrypto_sha1",		EVP_hcrypto_sha1());
    test_bulk_digest("hcrypto_sha256",		EVP_hcrypto_sha256());
    test_bulk_digest("hcrypto_sha384",		EVP_hcrypto_sha384());
    test_bulk_digest("hcrypto_sha512",		EVP_hcrypto_sha512());
}

#ifdef __APPLE__
static void
test_bulk_provider_cc(void)
{
    test_bulk_cipher("cc_aes_256_cbc",		EVP_cc_aes_256_cbc());
#if 0
    test_bulk_cipher("cc_aes_256_cfb8",		EVP_cc_aes_256_cfb8());
#endif
    test_bulk_cipher("cc_rc4",			EVP_cc_rc4());
    test_bulk_digest("cc_md2",			EVP_cc_md2());
    test_bulk_digest("cc_md4",			EVP_cc_md4());
    test_bulk_digest("cc_md5",			EVP_cc_md5());
    test_bulk_digest("cc_sha1",			EVP_cc_sha1());
    test_bulk_digest("cc_sha256",		EVP_cc_sha256());
    test_bulk_digest("cc_sha384",		EVP_cc_sha384());
    test_bulk_digest("cc_sha512",		EVP_cc_sha512());
}
#endif /* __APPLE__ */

#ifdef WIN32
static void
test_bulk_provider_w32crypto(void)
{
    test_bulk_cipher("w32crypto_aes_256_cbc",	EVP_w32crypto_aes_256_cbc());
#if 0
    test_bulk_cipher("w32crypto_aes_256_cfb8",	EVP_w32crypto_aes_256_cfb8());
#endif
    test_bulk_cipher("w32crypto_rc4",		EVP_w32crypto_rc4());
    test_bulk_digest("w32crypto_md2",		EVP_w32crypto_md2());
    test_bulk_digest("w32crypto_md4",		EVP_w32crypto_md4());
    test_bulk_digest("w32crypto_md5",		EVP_w32crypto_md5());
    test_bulk_digest("w32crypto_sha1",		EVP_w32crypto_sha1());
    test_bulk_digest("w32crypto_sha256",	EVP_w32crypto_sha256());
    test_bulk_digest("w32crypto_sha384",	EVP_w32crypto_sha384());
    test_bulk_digest("w32crypto_sha512",	EVP_w32crypto_sha512());
}
#endif /* WIN32 */

#if __sun || defined(PKCS11_MODULE_PATH)
static void
test_bulk_provider_pkcs11(void)
{
    test_bulk_cipher("pkcs11_aes_256_cbc",	EVP_pkcs11_aes_256_cbc());
    test_bulk_cipher("pkcs11_rc4",		EVP_pkcs11_rc4());
    test_bulk_digest("pkcs11_md5",		EVP_pkcs11_md5());
    test_bulk_digest("pkcs11_sha1",		EVP_pkcs11_sha1());
    test_bulk_digest("pkcs11_sha256",		EVP_pkcs11_sha256());
    test_bulk_digest("pkcs11_sha384",		EVP_pkcs11_sha384());
    test_bulk_digest("pkcs11_sha512",		EVP_pkcs11_sha512());
}
#endif /* __sun || PKCS11_MODULE_PATH */

int
main(int argc, char **argv)
{
    int ret = 0;
    int idx = 0;
    int i;

    setprogname(argv[0]);

    if(getarg(args, sizeof(args) / sizeof(args[0]), argc, argv, &idx))
	usage(1);

    if (help_flag)
	usage(0);

    if(version_flag) {
	print_version(NULL);
	exit(0);
    }

    argc -= idx;
    argv += idx;

    len *= 1024;

    d = emalloc(len);
    for (i = 0; i < len; i++)
        d[i] = i & 0xff;

    if (strcmp(provider, "hcrypto") == 0)
        test_bulk_provider_hcrypto();
#ifdef __APPLE__
    else if (strcmp(provider, "cc") == 0)
        test_bulk_provider_cc();
#endif
#ifdef WIN32
    else if (strcmp(provider, "w32crypto") == 0)
        test_bulk_provider_w32crypto();
#endif
#if __sun || defined(PKCS11_MODULE_PATH)
    else if (strcmp(provider, "pkcs11") == 0)
        test_bulk_provider_pkcs11();
#endif
    else
        usage(1);

    free(d);

    return ret;
}
