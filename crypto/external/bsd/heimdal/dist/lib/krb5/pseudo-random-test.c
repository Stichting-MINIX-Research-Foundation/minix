/*	$NetBSD: pseudo-random-test.c,v 1.2 2017/01/28 21:31:49 christos Exp $	*/

/*
 * Copyright (c) 2001 Kungliga Tekniska Högskolan
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
 * 3. Neither the name of KTH nor the names of its contributors may be
 *    used to endorse or promote products derived from this software without
 *    specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY KTH AND ITS CONTRIBUTORS ``AS IS'' AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL KTH OR ITS CONTRIBUTORS BE
 * LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR
 * BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY,
 * WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR
 * OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF
 * ADVISED OF THE POSSIBILITY OF SUCH DAMAGE. */

#include "krb5_locl.h"
#include <err.h>

enum { MAXSIZE = 48 };

static struct testcase {
    krb5_enctype enctype;
    unsigned char constant[MAXSIZE];
    size_t constant_len;
    unsigned char key[MAXSIZE];
    unsigned char res[MAXSIZE];
} tests[] = {
    {ETYPE_AES128_CTS_HMAC_SHA256_128, "test", 4,
     {0x37, 0x05, 0xD9, 0x60, 0x80, 0xC1, 0x77, 0x28, 0xA0, 0xE8, 0x00, 0xEA, 0xB6, 0xE0, 0xD2, 0x3C},
     {0x9D, 0x18, 0x86, 0x16, 0xF6, 0x38, 0x52, 0xFE, 0x86, 0x91, 0x5B, 0xB8, 0x40, 0xB4, 0xA8, 0x86,
      0xFF, 0x3E, 0x6B, 0xB0, 0xF8, 0x19, 0xB4, 0x9B, 0x89, 0x33, 0x93, 0xD3, 0x93, 0x85, 0x42, 0x95}},
    {ETYPE_AES256_CTS_HMAC_SHA384_192, "test", 4,
     {0x6D, 0x40, 0x4D, 0x37, 0xFA, 0xF7, 0x9F, 0x9D, 0xF0, 0xD3, 0x35, 0x68, 0xD3, 0x20, 0x66, 0x98,
      0x00, 0xEB, 0x48, 0x36, 0x47, 0x2E, 0xA8, 0xA0, 0x26, 0xD1, 0x6B, 0x71, 0x82, 0x46, 0x0C, 0x52},
     {0x98, 0x01, 0xF6, 0x9A, 0x36, 0x8C, 0x2B, 0xF6, 0x75, 0xE5, 0x95, 0x21, 0xE1, 0x77, 0xD9, 0xA0,
      0x7F, 0x67, 0xEF, 0xE1, 0xCF, 0xDE, 0x8D, 0x3C, 0x8D, 0x6F, 0x6A, 0x02, 0x56, 0xE3, 0xB1, 0x7D,
      0xB3, 0xC1, 0xB6, 0x2A, 0xD1, 0xB8, 0x55, 0x33, 0x60, 0xD1, 0x73, 0x67, 0xEB, 0x15, 0x14, 0xD2}},
    {0, {0}, 0, {0}, {0}}
};

int
main(int argc, char **argv)
{
    struct testcase *t;
    krb5_context context;
    krb5_error_code ret;
    int val = 0;

    ret = krb5_init_context (&context);
    if (ret)
	errx (1, "krb5_init_context failed: %d", ret);

    for (t = tests; t->enctype != 0; ++t) {
	krb5_keyblock key;
	krb5_crypto crypto;
	krb5_data constant, prf;

	krb5_data_zero(&prf);

	key.keytype = t->enctype;
	krb5_enctype_keysize(context, t->enctype, &key.keyvalue.length);
	key.keyvalue.data   = t->key;

	ret = krb5_crypto_init(context, &key, 0, &crypto);
	if (ret)
	    krb5_err (context, 1, ret, "krb5_crypto_init");

	constant.data = t->constant;
	constant.length = t->constant_len;

	ret = krb5_crypto_prf(context, crypto, &constant, &prf);
	if (ret)
	    krb5_err (context, 1, ret, "krb5_crypto_prf");

	if (memcmp(prf.data, t->res, prf.length) != 0) {
	    const unsigned char *p = prf.data;
	    int i;

	    printf ("PRF failed (enctype %d)\n", t->enctype);
	    printf ("should be: ");
	    for (i = 0; i < prf.length; ++i)
		printf ("%02x", t->res[i]);
	    printf ("\nresult was: ");
	    for (i = 0; i < prf.length; ++i)
		printf ("%02x", p[i]);
	    printf ("\n");
	    val = 1;
	}
	krb5_data_free(&prf);
	krb5_crypto_destroy(context, crypto);
    }
    krb5_free_context(context);

    return val;
}
