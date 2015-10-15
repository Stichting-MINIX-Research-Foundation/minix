/*	$NetBSD: apop.c,v 1.1.1.2 2014/04/24 12:45:51 pettai Exp $	*/

/*
 * Copyright (c) 2010 Kungliga Tekniska Högskolan
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

#include <sys/types.h>
#include <stdio.h>
#include <unistd.h>
#include <CommonCrypto/CommonDigest.h>
#include <CommonCrypto/CommonHMAC.h>
#include <krb5/roken.h>
#include <krb5/hex.h>
#include "heim-auth.h"
#include <krb5/ntlm_err.h>

char *
heim_generate_challenge(const char *hostname)
{
    char host[MAXHOSTNAMELEN], *str = NULL;
    uint32_t num, t;

    if (hostname == NULL) {
	if (gethostname(host, sizeof(host)))
	    return NULL;
	hostname = host;
    }

    t = time(NULL);
    num = rk_random();

    asprintf(&str, "<%lu%lu@%s>", (unsigned long)t,
	     (unsigned long)num, hostname);

    return str;
}

char *
heim_apop_create(const char *challenge, const char *password)
{
    char *str = NULL;
    uint8_t hash[CC_MD5_DIGEST_LENGTH];
    CC_MD5_CTX ctx;

    CC_MD5_Init(&ctx);
    CC_MD5_Update(&ctx, challenge, strlen(challenge));
    CC_MD5_Update(&ctx, password, strlen(password));

    CC_MD5_Final(hash, &ctx);

    hex_encode(hash, sizeof(hash), &str);
    if (str)
      strlwr(str);

    return str;
}

int
heim_apop_verify(const char *challenge, const char *password, const char *response)
{
    char *str;
    int res;

    str = heim_apop_create(challenge, password);
    if (str == NULL)
	return ENOMEM;

    res = (strcasecmp(str, response) != 0);
    free(str);

    if (res)
	return HNTLM_ERR_INVALID_APOP;
    return 0;
}

struct heim_cram_md5 {
    CC_MD5_CTX ipad;
    CC_MD5_CTX opad;
};


void
heim_cram_md5_export(const char *password, heim_CRAM_MD5_STATE *state)
{
    size_t keylen = strlen(password);
    uint8_t key[CC_MD5_BLOCK_BYTES];
    uint8_t pad[CC_MD5_BLOCK_BYTES];
    struct heim_cram_md5 ctx;
    size_t n;

    memset(&ctx, 0, sizeof(ctx));

    if (keylen > CC_MD5_BLOCK_BYTES) {
	CC_MD5(password, keylen, key);
	keylen = sizeof(keylen);
    } else {
	memcpy(key, password, keylen);
    }

    memset(pad, 0x36, sizeof(pad));
    for (n = 0; n < keylen; n++)
	pad[n] ^= key[n];

    CC_MD5_Init(&ctx.ipad);
    CC_MD5_Init(&ctx.opad);

    CC_MD5_Update(&ctx.ipad, pad, sizeof(pad));

    memset(pad, 0x5c, sizeof(pad));
    for (n = 0; n < keylen; n++)
	pad[n] ^= key[n];

    CC_MD5_Update(&ctx.opad, pad, sizeof(pad));

    memset(pad, 0, sizeof(pad));
    memset(key, 0, sizeof(key));

    state->istate[0] = htonl(ctx.ipad.A);
    state->istate[1] = htonl(ctx.ipad.B);
    state->istate[2] = htonl(ctx.ipad.C);
    state->istate[3] = htonl(ctx.ipad.D);

    state->ostate[0] = htonl(ctx.opad.A);
    state->ostate[1] = htonl(ctx.opad.B);
    state->ostate[2] = htonl(ctx.opad.C);
    state->ostate[3] = htonl(ctx.opad.D);

    memset(&ctx, 0, sizeof(ctx));
}


heim_cram_md5
heim_cram_md5_import(void *data, size_t len)
{
    heim_CRAM_MD5_STATE state;
    heim_cram_md5 ctx;
    unsigned n;

    if (len != sizeof(state))
	return NULL;

    ctx = calloc(1, sizeof(*ctx));
    if (ctx == NULL)
	return NULL;

    memcpy(&state, data, sizeof(state));

    ctx->ipad.A = ntohl(state.istate[0]);
    ctx->ipad.B = ntohl(state.istate[1]);
    ctx->ipad.C = ntohl(state.istate[2]);
    ctx->ipad.D = ntohl(state.istate[3]);

    ctx->opad.A = ntohl(state.ostate[0]);
    ctx->opad.B = ntohl(state.ostate[1]);
    ctx->opad.C = ntohl(state.ostate[2]);
    ctx->opad.D = ntohl(state.ostate[3]);

    ctx->ipad.Nl = ctx->opad.Nl = 512;
    ctx->ipad.Nh = ctx->opad.Nh = 0;
    ctx->ipad.num = ctx->opad.num = 0;

    return ctx;
}

int
heim_cram_md5_verify_ctx(heim_cram_md5 ctx, const char *challenge, const char *response)
{
    uint8_t hash[CC_MD5_DIGEST_LENGTH];
    char *str = NULL;
    int res;

    CC_MD5_Update(&ctx->ipad, challenge, strlen(challenge));
    CC_MD5_Final(hash, &ctx->ipad);

    CC_MD5_Update(&ctx->opad, hash, sizeof(hash));
    CC_MD5_Final(hash, &ctx->opad);

    hex_encode(hash, sizeof(hash), &str);
    if (str == NULL)
	return ENOMEM;

    res = (strcasecmp(str, response) != 0);
    free(str);

    if (res)
	return HNTLM_ERR_INVALID_CRAM_MD5;
    return 0;
}

void
heim_cram_md5_free(heim_cram_md5 ctx)
{
    memset(ctx, 0, sizeof(*ctx));
    free(ctx);
}


char *
heim_cram_md5_create(const char *challenge, const char *password)
{
    CCHmacContext ctx;
    uint8_t hash[CC_MD5_DIGEST_LENGTH];
    char *str = NULL;

    CCHmacInit(&ctx, kCCHmacAlgMD5, password, strlen(password));
    CCHmacUpdate(&ctx, challenge, strlen(challenge));
    CCHmacFinal(&ctx, hash);

    memset(&ctx, 0, sizeof(ctx));

    hex_encode(hash, sizeof(hash), &str);
    if (str)
      strlwr(str);

    return str;
}

 int
heim_cram_md5_verify(const char *challenge, const char *password, const char *response)
{
    char *str;
    int res;

    str = heim_cram_md5_create(challenge, password);
    if (str == NULL)
	return ENOMEM;

    res = (strcasecmp(str, response) != 0);
    free(str);

    if (res)
	return HNTLM_ERR_INVALID_CRAM_MD5;
    return 0;
}

