/* $NetBSD: t_crypto.c,v 1.4 2011/02/12 23:21:33 christos Exp $ */

/* Copyright (c) 2010 The NetBSD Foundation, Inc.
 * All rights reserved.
 *
 * This code is derived from software contributed to The NetBSD Foundation
 * by Mateusz Kocielski.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 3. All advertising materials mentioning features or use of this software
 *    must display the following acknowledgement:
 *  	  This product includes software developed by the NetBSD
 *  	  Foundation, Inc. and its contributors.
 * 4. Neither the name of The NetBSD Foundation nor the names of its
 *    contributors may be used to endorse or promote products derived
 *    from this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE NETBSD FOUNDATION, INC. AND CONTRIBUTORS
 * ``AS IS'' AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED
 * TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED.	IN NO EVENT SHALL THE FOUNDATION OR CONTRIBUTORS
 * BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 * CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
 * POSSIBILITY OF SUCH DAMAGE.
 */
#include <sys/cdefs.h>
__RCSID("$NetBSD: t_crypto.c,v 1.4 2011/02/12 23:21:33 christos Exp $");

#include <atf-c.h>
#include <saslc.h>
#include <stdio.h>
#include <string.h>

#include "crypto.h"


typedef struct {
	char *in;
	char *key;
	char *out;
} hmac_md5_test_case_t;

#define HMAC_MD5_TEST_CASES 5
hmac_md5_test_case_t hmac_md5_test_cases[HMAC_MD5_TEST_CASES] = {
	{ /* taken from the RFC2195 */
		"<1896.697170952@postoffice.reston.mci.net>" /* in */,
		"tanstaaftanstaaf" /* key */,
		"b913a602c7eda7a495b4e6e7334d3890" /* out */
	},
	{ /* taken from the draft-ietf-sasl-crammd5 */
		"<1896.697170952@postoffice.example.net>" /* in */,
		"tanstaaftanstaaf" /* key */,
		"3dbc88f0624776a737b39093f6eb6427" /* out */
	},
	{
		"<68451038525716401353.0@localhost>" /* in */,
		"Open, Sesame" /* key */,
		"6fa32b6e768f073132588e3418e00f71" /* out */
	},
	{ /* taken from RFC2104 */
		"what do ya want for nothing?" /* in */,
		"Jefe" /* key */,
		"750c783e6ab0b503eaa86e310a5db738" /* out */
	},
	{ /* taken from RFC2202 */
		"Test Using Larger Than Block-Size Key - Hash Key First" /* in */,
		"\xAA\xAA\xAA\xAA\xAA\xAA\xAA\xAA\xAA\xAA\xAA\xAA"
		"\xAA\xAA\xAA\xAA\xAA\xAA\xAA\xAA\xAA\xAA\xAA\xAA"
		"\xAA\xAA\xAA\xAA\xAA\xAA\xAA\xAA\xAA\xAA\xAA\xAA"
		"\xAA\xAA\xAA\xAA\xAA\xAA\xAA\xAA\xAA\xAA\xAA\xAA"
		"\xAA\xAA\xAA\xAA\xAA\xAA\xAA\xAA\xAA\xAA\xAA\xAA"
		"\xAA\xAA\xAA\xAA\xAA\xAA\xAA\xAA\xAA\xAA\xAA\xAA"
		"\xAA\xAA\xAA\xAA\xAA\xAA\xAA\xAA" /* key */,
		"6b1ab7fe4bd7bf8f0b62e6ce61b9d0cd" /* out */
	}
};

typedef struct {
	char *in;
	char *out;
} md5_test_case_t;

#define MD5_TEST_CASES 4
md5_test_case_t md5_test_cases[HMAC_MD5_TEST_CASES] = {
	{
		"this is very hard test" /* in */,
		"c9145ff35600132e4c9b821e19c27783" /* out */
	},
	{
		"shm" /* in */,
		"99ebb038380e15bc896c3a17733ab484" /* out */
	},
	{
		"this is a bit longer test, isn't it?" /* in */,
		"b69f7a5e9c1f701ed90033b87ccca94c" /* out */
	},
	{
		"OK, enough",
		"fecb46e815d0ba4b7c89d050e30124ea" /* out */
	}
};

typedef struct {
	char *in;
	char *out;
	size_t len;
} base64_test_case_t;

#define BASE64_TEST_CASES 4
base64_test_case_t base64_test_cases[BASE64_TEST_CASES] = {
	{
		"this is very hard test" /* in */,
		"dGhpcyBpcyB2ZXJ5IGhhcmQgdGVzdA==" /* out */,
		22 /* len */
	},
	{
		"shm" /* in */,
		"c2ht" /* out */,
		3 /* len */
	},
	{
		"this is a bit longer test, isn't it?" /* in */,
		"dGhpcyBpcyBhIGJpdCBsb25nZXIgdGVzdCwgaXNuJ3QgaXQ/" /* out */,
		36 /* len */
	},
	{
		"OK, enough",
		"T0ssIGVub3VnaA==" /* out */,
		10 /* len */
	}
};

ATF_TC(t_crypto_hmac_md5);
ATF_TC_HEAD(t_crypto_hmac_md5, tc)
{
	atf_tc_set_md_var(tc, "descr", "saslc__crypto_hmac_md5() tests");
}
ATF_TC_BODY(t_crypto_hmac_md5, tc)
{
	const char *digest;
	int i;

	for (i = 0; i < HMAC_MD5_TEST_CASES; i++) {
		digest = saslc__crypto_hmac_md5_hex(hmac_md5_test_cases[i].key,
		    strlen(hmac_md5_test_cases[i].key), hmac_md5_test_cases[i].in,
		    strlen(hmac_md5_test_cases[i].in));
		ATF_CHECK_STREQ_MSG(digest, hmac_md5_test_cases[i].out,
		    "saslc__crypto_hmac_md5_hex() failed on %s %s got %s should be: %s",
		    hmac_md5_test_cases[i].in, hmac_md5_test_cases[i].key, digest,
		    hmac_md5_test_cases[i].out);
		free((void *)digest);
	}
}

ATF_TC(t_crypto_md5);
ATF_TC_HEAD(t_crypto_md5, tc)
{

	atf_tc_set_md_var(tc, "descr", "saslc__hmac_md5_hex() tests");
}
ATF_TC_BODY(t_crypto_md5, tc)
{
	const char *digest;
	int i;

	for (i = 0; i < MD5_TEST_CASES; i++) {
		digest = saslc__crypto_md5_hex(md5_test_cases[i].in,
		    strlen(md5_test_cases[i].in));
		ATF_CHECK_STREQ_MSG(digest, md5_test_cases[i].out,
		    "saslc__crypto_md5_hex() failed on %s got %s should be: %s",
		    md5_test_cases[i].in, digest, md5_test_cases[i].out);
		free((void *)digest);
	}
}

ATF_TC(t_crypto_base64);
ATF_TC_HEAD(t_crypto_base64, tc)
{
	atf_tc_set_md_var(tc, "descr", "saslc__crypto_nonce() tests");
}
ATF_TC_BODY(t_crypto_base64, tc)
{
	char *enc;
	size_t enclen;
	int i;

	for (i = 0; i < BASE64_TEST_CASES; i++) {
		saslc__crypto_encode_base64(base64_test_cases[i].in,
		    base64_test_cases[i].len, &enc, &enclen);
		ATF_CHECK_STREQ_MSG(enc, base64_test_cases[i].out,
		    "saslc__crypto_encode_base64() failed on %s got %s should be: %s",
		    base64_test_cases[i].in, enc, base64_test_cases[i].out);
		free((void *)enc);
	}
}

ATF_TC(t_crypto_nonce);
ATF_TC_HEAD(t_crypto_nonce, tc)
{

	atf_tc_set_md_var(tc, "descr", "saslc__crypto_nonce() tests");
}
ATF_TC_BODY(t_crypto_nonce, tc)
{
	unsigned char *x, *y;

	/* Any better ideas how to test that? ... */

	x = saslc__crypto_nonce(1024);
	y = saslc__crypto_nonce(1024);

	ATF_CHECK_EQ(((strncmp(x, y, 1024) == 0) ? 1 : 0), 0);

	free(x);
	free(y);
}

ATF_TP_ADD_TCS(tp)
{

	ATF_TP_ADD_TC(tp, t_crypto_hmac_md5);
	ATF_TP_ADD_TC(tp, t_crypto_md5);
	ATF_TP_ADD_TC(tp, t_crypto_base64);
	ATF_TP_ADD_TC(tp, t_crypto_nonce);
	return atf_no_error();
}
