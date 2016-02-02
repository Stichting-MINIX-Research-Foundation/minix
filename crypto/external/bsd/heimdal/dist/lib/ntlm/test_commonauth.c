/*	$NetBSD: test_commonauth.c,v 1.1.1.2 2014/04/24 12:45:51 pettai Exp $	*/

/*
 * Copyright (c) 2006 - 2008 Kungliga Tekniska Högskolan
 * (Royal Institute of Technology, Stockholm, Sweden).
 * All rights reserved.
 *
 * Portions Copyright (c) 2010 Apple Inc. All rights reserved.
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

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <krb5/roken.h>
#include "heim-auth.h"

static int
test_sasl_digest_md5(void)
{
    heim_digest_t ctx;
    const char *user;
    char *r;

    if ((ctx = heim_digest_create(1, HEIM_DIGEST_TYPE_AUTO)) == NULL)
	abort();

    if (heim_digest_parse_challenge(ctx, "realm=\"elwood.innosoft.com\",nonce=\"OA6MG9tEQGm2hh\",qop=\"auth\",algorithm=md5-sess,charset=utf-8"))
	abort();
    if (heim_digest_parse_response(ctx, "charset=utf-8,username=\"chris\",realm=\"elwood.innosoft.com\",nonce=\"OA6MG9tEQGm2hh\",nc=00000001,cnonce=\"OA6MHXh6VqTrRk\",digest-uri=\"imap/elwood.innosoft.com\",response=d388dad90d4bbd760a152321f2143af7,qop=auth"))
	abort();

    if ((user = heim_digest_get_key(ctx, "username")) == NULL)
	abort();
    if (strcmp(user, "chris") != 0)
	abort();

    heim_digest_set_key(ctx, "password", "secret");

    if (heim_digest_verify(ctx, &r))
	abort();

    if (strcmp(r, "rspauth=ea40f60335c427b5527b84dbabcdfffd") != 0)
	abort();

    free(r);

    heim_digest_release(ctx);

    return 0;
}

static int
test_http_digest_md5(void)
{
    heim_digest_t ctx;
    const char *user;

    if ((ctx = heim_digest_create(1, HEIM_DIGEST_TYPE_AUTO)) == NULL)
	abort();

    if (heim_digest_parse_challenge(ctx, "realm=\"testrealm@host.com\","
				    "nonce=\"dcd98b7102dd2f0e8b11d0f600bfb0c093\","
				    "opaque=\"5ccc069c403ebaf9f0171e9517f40e41\""))
	abort();

    if (heim_digest_parse_response(ctx, "username=\"Mufasa\","
				   "realm=\"testrealm@host.com\","
				   "nonce=\"dcd98b7102dd2f0e8b11d0f600bfb0c093\","
				   "uri=\"/dir/index.html\","
				   "response=\"1949323746fe6a43ef61f9606e7febea\","
				   "opaque=\"5ccc069c403ebaf9f0171e9517f40e41\""))
	abort();

    if ((user = heim_digest_get_key(ctx, "username")) == NULL)
	abort();
    if (strcmp(user, "Mufasa") != 0)
	abort();

    heim_digest_set_key(ctx, "password", "CircleOfLife");

    if (heim_digest_verify(ctx, NULL))
	abort();

    heim_digest_release(ctx);

    return 0;
}

static int
test_cram_md5(void)
{
    const char *chal = "<1896.697170952@postoffice.reston.mci.net>";
    const char *secret = "tanstaaftanstaaf";
    const char *resp = "b913a602c7eda7a495b4e6e7334d3890";
    heim_CRAM_MD5_STATE state;
    heim_cram_md5 ctx;
    char *t;

    const uint8_t *prestate = (uint8_t *)
	"\x87\x1E\x24\x10\xB4\x0C\x72\x5D\xA3\x95\x2D\x5B\x8B\xFC\xDD\xE1"
	"\x29\x90\xCB\xA7\x66\xF6\xB3\x40\xE8\xAC\x48\x2C\xE4\xE3\xA4\x40";

    /*
     * Test prebuild blobs
     */

    if (sizeof(state) != 32)
	abort();

    heim_cram_md5_export("foo", &state);

    if (memcmp(prestate, &state, 32) != 0)
	abort();

    /*
     * Check example
     */


    if (heim_cram_md5_verify(chal, secret, resp) != 0)
	abort();


    /*
     * Do it ourself
     */

    t = heim_cram_md5_create(chal, secret);
    if (t == NULL)
	abort();

    if (strcmp(resp, t) != 0)
	abort();

    heim_cram_md5_export(secret, &state);

    /* here you can store the memcpy-ed version of state somewhere else */

    ctx = heim_cram_md5_import(&state, sizeof(state));

    memset(&state, 0, sizeof(state));

    if (heim_cram_md5_verify_ctx(ctx, chal, resp) != 0)
	abort();

    heim_cram_md5_free(ctx);

    free(t);

    return 0;
}

static int
test_apop(void)
{
    const char *chal = "<1896.697170952@dbc.mtview.ca.us>";
    const char *secret = "tanstaaf";
    const char *resp = "c4c9334bac560ecc979e58001b3e22fb";
    char *t;


    t = heim_apop_create(chal, secret);
    if (t == NULL)
	abort();

    if (strcmp(resp, t) != 0)
	abort();

    if (heim_apop_verify(chal, secret, resp) != 0)
	abort();

    free(t);

    return 0;
}


int
main(int argc, char **argv)
{
    int ret = 0;

    ret |= test_sasl_digest_md5();
    ret |= test_http_digest_md5();
    ret |= test_cram_md5();
    ret |= test_apop();

    system("bash");

    return ret;
}
