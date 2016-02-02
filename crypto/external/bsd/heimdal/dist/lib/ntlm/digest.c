/*	$NetBSD: digest.c,v 1.1.1.2 2014/04/24 12:45:51 pettai Exp $	*/

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

#include <sys/types.h>
#include <stdio.h>
#include <unistd.h>
#include <CommonCrypto/CommonDigest.h>
#include <CommonCrypto/CommonHMAC.h>
#include <assert.h>
#include <krb5/roken.h>
#include <krb5/hex.h>
#include "heim-auth.h"
#include <krb5/ntlm_err.h>

struct heim_digest_desc {
    int server;
    int type;
    char *password;
    uint8_t SecretHash[CC_MD5_DIGEST_LENGTH];
    char *serverNonce;
    char *serverRealm;
    char *serverQOP;
    char *serverMethod;
    char *clientUsername;
    char *clientResponse;
    char *clientURI;
    char *clientRealm;
    char *clientNonce;
    char *clientQOP;
    char *clientNC;
    char *serverAlgorithm;
    char *auth_id;
};

#define FREE_AND_CLEAR(x) do { if ((x)) { free((x)); (x) = NULL; } } while(0)
#define MEMSET_FREE_AND_CLEAR(x) do { if ((x)) { memset(x, 0, strlen(x)); free((x)); (x) = NULL; } } while(0)

static void
clear_context(heim_digest_t context)
{
    MEMSET_FREE_AND_CLEAR(context->password);
    memset(context->SecretHash, 0, sizeof(context->SecretHash));
    FREE_AND_CLEAR(context->serverNonce);
    FREE_AND_CLEAR(context->serverRealm);
    FREE_AND_CLEAR(context->serverQOP);
    FREE_AND_CLEAR(context->serverMethod);
    FREE_AND_CLEAR(context->clientUsername);
    FREE_AND_CLEAR(context->clientResponse);
    FREE_AND_CLEAR(context->clientURI);
    FREE_AND_CLEAR(context->clientRealm);
    FREE_AND_CLEAR(context->clientNonce);
    FREE_AND_CLEAR(context->clientQOP);
    FREE_AND_CLEAR(context->clientNC);
    FREE_AND_CLEAR(context->serverAlgorithm);
    FREE_AND_CLEAR(context->auth_id);
}

static char *
build_A1_hash(int type,
	      const char *username, const char *password,
	      const char *realm, const char *serverNonce,
	      const char *clientNonce,
	      const char *auth_id)
{
    unsigned char md[CC_MD5_DIGEST_LENGTH];
    CC_MD5_CTX ctx;
    char *A1;

    CC_MD5_Init(&ctx);
    CC_MD5_Update(&ctx, username, strlen(username));
    CC_MD5_Update(&ctx, ":", 1);
    CC_MD5_Update(&ctx, realm, strlen(realm));
    CC_MD5_Update(&ctx, ":", 1);
    CC_MD5_Update(&ctx, password, strlen(password));
    CC_MD5_Final(md, &ctx);

    if (type != HEIM_DIGEST_TYPE_RFC2069) {
	CC_MD5_Init(&ctx);
	CC_MD5_Update(&ctx, md, sizeof(md));
	memset(md, 0, sizeof(md));
	CC_MD5_Update(&ctx, ":", 1);
	CC_MD5_Update(&ctx, serverNonce, strlen(serverNonce));
	if (clientNonce) {
	    CC_MD5_Update(&ctx, ":", 1);
	    CC_MD5_Update(&ctx, clientNonce, strlen(clientNonce));
	}
	if (auth_id) {
	    CC_MD5_Update(&ctx, ":", 1);
	    CC_MD5_Update(&ctx, auth_id, strlen(auth_id));
	}
	CC_MD5_Final(md, &ctx);
    }
    hex_encode(md, sizeof(md), &A1);
    if (A1)
      strlwr(A1);

    return A1;
}

static char *
build_A2_hash(heim_digest_t context, const char *method)
{
    unsigned char md[CC_MD5_DIGEST_LENGTH];
    CC_MD5_CTX ctx;
    char *A2;

    CC_MD5_Init(&ctx);
    if (method)
	CC_MD5_Update(&ctx, method, strlen(method));
    CC_MD5_Update(&ctx, ":", 1);
    CC_MD5_Update(&ctx, context->clientURI, strlen(context->clientURI));

    /* conf|int */
    if (context->type != HEIM_DIGEST_TYPE_RFC2069) {
	if (strcmp(context->clientQOP, "auth") != 0) {
	    /* XXX if we have a body hash, use that */
	    static char conf_zeros[] = ":00000000000000000000000000000000";
	    CC_MD5_Update(&ctx, conf_zeros, sizeof(conf_zeros) - 1);
	}
    } else {
	/* support auth-int ? */
    }

    CC_MD5_Final(md, &ctx);

    hex_encode(md, sizeof(md), &A2);
    if (A2)
      strlwr(A2);

    return A2;
}

/*
 *
 */

struct md5_value {
    char		*mv_name;
    char		*mv_value;
    struct md5_value 	*mv_next;
};

static void
free_values(struct md5_value *val)
{
    struct md5_value *v;
    while(val) {
	v = val->mv_next;
	if (val->mv_name)
	    free(val->mv_name);
	if (val->mv_value)
	    free(val->mv_value);
	free(val);
	val = v;
    }
}

/*
 * Search for entry, if found, remove entry and return string to be freed.
 */

static char *
values_find(struct md5_value **val, const char *v)
{
    struct md5_value *cur = *val;
    char *str;

    while (*val != NULL) {
	if (strcasecmp(v, (*val)->mv_name) == 0)
	    break;
	val = &(*val)->mv_next;
    }
    if (*val == NULL)
	return NULL;
    cur = *val;
    *val = (*val)->mv_next;

    str = cur->mv_value;
    free(cur->mv_name);
    free(cur);

    return str;
}

static int
parse_values(const char *string, struct md5_value **val)
{
    struct md5_value *v;
    size_t size;
    char *str, *p1, *p2;
    size_t sz;

    *val = NULL;

    if ((str = strdup(string)) == NULL)
	return ENOMEM;

    size = strlen(str);

    p1 = str;

    while (p1 - str < size) {
	sz = strspn(p1, " \t\n\r,");
	if (p1[sz] == '\0')
	    break;
	p1 += sz;
	sz = strcspn(p1, " \t\n\r=");
	if (sz == 0 || p1[sz] == '\0')
	    goto error;
	p2 = p1 + sz;

	if ((v = malloc(sizeof(*v))) == NULL)
	    goto nomem;
	v->mv_name = v->mv_value = NULL;
	v->mv_next = *val;
	*val = v;
	if ((v->mv_name = malloc(p2 - p1 + 1)) == NULL)
	    goto nomem;
	strncpy(v->mv_name, p1, p2 - p1);
	v->mv_name[p2 - p1] = '\0';

	sz = strspn(p2, " \t\n\r");
	if (p2[sz] == '\0')
	    goto error;
	p2 += sz;

	if (*p2 != '=')
	    goto error;
	p2++;

	sz = strspn(p2, " \t\n\r");
	if (p2[sz] == '\0')
	    goto error;
	p2 += sz;
	p1 = p2;

	if (*p2 == '"') {
	    p1++;
	    while (*p2 == '"') {
		p2++;
		p2 = strchr(p2, '\"');
		if (p2 == NULL)
		    goto error;
		if (p2[0] == '\0')
		    goto error;
		if (p2[-1] != '\\')
		    break;
	    }
	} else {
	    sz = strcspn(p2, " \t\n\r=,");
	    p2 += sz;
	}

#if 0 /* allow empty values */
	if (p1 == p2)
	    goto error;
#endif

	if ((v->mv_value = malloc(p2 - p1 + 1)) == NULL)
	    goto nomem;
	strncpy(v->mv_value, p1, p2 - p1);
	v->mv_value[p2 - p1] = '\0';

	if (p2[0] == '\0')
	    break;
	if (p2[0] == '"')
	    p2++;

	sz = strspn(p2, " \t\n\r");
	if (p2[sz] == '\0')
	    break;
	p2 += sz;

	if (p2[0] == '\0')
	    break;
	if (p2[0] != ',')
	    goto error;
	p1 = p2;
    }

    free(str);

    return 0;
 error:
    free_values(*val);
    *val = NULL;
    free(str);
    return EINVAL;
 nomem:
    free_values(*val);
    *val = NULL;
    free(str);
    return ENOMEM;
}

/*
 *
 */

heim_digest_t
heim_digest_create(int server, int type)
{
    heim_digest_t context;

    context = calloc(1, sizeof(*context));
    if (context == NULL)
	return NULL;
    context->server = server;
    context->type = type;

    return context;
}

const char *
heim_digest_generate_challenge(heim_digest_t context)
{
    return NULL;
}

int
heim_digest_parse_challenge(heim_digest_t context, const char *challenge)
{
    struct md5_value *val = NULL;
    int ret, type;

    ret = parse_values(challenge, &val);
    if (ret)
	goto out;

    ret = 1;

    context->serverNonce = values_find(&val, "nonce");
    if (context->serverNonce == NULL) goto out;

    context->serverRealm = values_find(&val, "realm");
    if (context->serverRealm == NULL) goto out;

    context->serverQOP = values_find(&val, "qop");
    if (context->serverQOP == NULL)
	context->serverQOP = strdup("auth");
    if (context->serverQOP == NULL) goto out;

    /* check alg */

    context->serverAlgorithm = values_find(&val, "algorithm");
    if (context->serverAlgorithm == NULL || strcasecmp(context->serverAlgorithm, "md5") == 0) {
	type = HEIM_DIGEST_TYPE_RFC2069;
    } else if (strcasecmp(context->serverAlgorithm, "md5-sess") == 0) {
	type = HEIM_DIGEST_TYPE_MD5_SESS;
    } else {
	goto out;
    }

    if (context->type != HEIM_DIGEST_TYPE_AUTO && context->type != type)
	goto out;
    else
	context->type = type;



    ret = 0;
 out:
    free_values(val);
    if (ret)
	clear_context(context);
    return ret;
}

int
heim_digest_parse_response(heim_digest_t context, const char *response)
{
    struct md5_value *val = NULL;
    char *nonce;
    int ret;

    ret = parse_values(response, &val);
    if (ret)
	goto out;

    ret = 1;

    if (context->type == HEIM_DIGEST_TYPE_AUTO)
	goto out;

    context->clientUsername = values_find(&val, "username");
    if (context->clientUsername == NULL) goto out;

    context->clientRealm = values_find(&val, "realm");

    context->clientResponse = values_find(&val, "response");
    if (context->clientResponse == NULL) goto out;

    nonce = values_find(&val, "nonce");
    if (nonce == NULL) goto out;

    if (strcmp(nonce, context->serverNonce) != 0) {
	free(nonce);
	goto out;
    }
    free(nonce);

    context->clientQOP = values_find(&val, "qop");
    if (context->clientQOP == NULL)
	context->clientQOP = strdup("auth");
    if (context->clientQOP == NULL) goto out;


    if (context->type != HEIM_DIGEST_TYPE_RFC2069) {
	context->clientNC = values_find(&val, "nc");
	if (context->clientNC == NULL) goto out;

	context->clientNonce = values_find(&val, "cnonce");
	if (context->clientNonce == NULL) goto out;
    }

    if (context->type == HEIM_DIGEST_TYPE_RFC2069)
	context->clientURI = values_find(&val, "uri");
    else
	context->clientURI = values_find(&val, "digest-uri");
    if (context->clientURI == NULL) goto out;

    ret = 0;
 out:
    free_values(val);
    return ret;
}

const char *
heim_digest_get_key(heim_digest_t context, const char *key)
{
    if (strcmp(key, "username") == 0) {
        return context->clientUsername;
    } else if (strcmp(key, "realm") == 0) {
        return context->clientRealm;
    } else {
	return NULL;
    }
}

int
heim_digest_set_key(heim_digest_t context, const char *key, const char *value)
{
    if (strcmp(key, "password") == 0) {
	FREE_AND_CLEAR(context->password);
	if ((context->password = strdup(value)) == NULL)
	    return ENOMEM;
    } else if (strcmp(key, "method") == 0) {
	FREE_AND_CLEAR(context->serverMethod);
	if ((context->serverMethod = strdup(value)) != NULL)
	    return ENOMEM;
    } else {
	return EINVAL;
    }
    return 0;
}

static char *
build_digest(heim_digest_t context, const char *a1, const char *method)
{
    CC_MD5_CTX ctx;
    uint8_t md[CC_MD5_DIGEST_LENGTH];
    char *a2, *str = NULL;

    a2 = build_A2_hash(context, method);
    if (a2 == NULL)
      return NULL;

    CC_MD5_Init(&ctx);
    CC_MD5_Update(&ctx, a1, strlen(a1));
    CC_MD5_Update(&ctx, ":", 1);
    CC_MD5_Update(&ctx, context->serverNonce, strlen(context->serverNonce));
    if (context->type != HEIM_DIGEST_TYPE_RFC2069) {
	CC_MD5_Update(&ctx, ":", 1);
	CC_MD5_Update(&ctx, context->clientNC, strlen(context->clientNC));
	CC_MD5_Update(&ctx, ":", 1);
	CC_MD5_Update(&ctx, context->clientNonce, strlen(context->clientNonce));
	CC_MD5_Update(&ctx, ":", 1);
	CC_MD5_Update(&ctx, context->clientQOP, strlen(context->clientQOP));
    }
    CC_MD5_Update(&ctx, ":", 1);
    CC_MD5_Update(&ctx, a2, strlen(a2));
    CC_MD5_Final(md, &ctx);

    free(a2);

    hex_encode(md, sizeof(md), &str);
    if (str)
      strlwr(str);

    return str;
}

const char *
heim_digest_create_response(heim_digest_t context)
{
    return NULL;
}

int
heim_digest_verify(heim_digest_t context, char **response)
{
    CC_MD5_CTX ctx;
    char *a1, *a2;
    uint8_t md[CC_MD5_DIGEST_LENGTH];
    char *str;
    int res;

    if (response)
	*response = NULL;

    if (context->serverMethod == NULL) {
	if (context->type != HEIM_DIGEST_TYPE_RFC2069)
	    context->serverMethod = strdup("AUTHENTICATE");
	else
	    context->serverMethod = strdup("GET");
    }

    a1 = build_A1_hash(context->type,
		       context->clientUsername, context->password,
		       context->serverRealm, context->serverNonce,
		       context->clientNonce, context->auth_id);
    if (a1 == NULL)
      return ENOMEM;

    str = build_digest(context, a1, context->serverMethod);
    if (str == NULL) {
	MEMSET_FREE_AND_CLEAR(a1);
	return ENOMEM;
    }

    res = (strcmp(str, context->clientResponse) == 0) ? 0 : EINVAL;
    free(str);
    if (res) {
	MEMSET_FREE_AND_CLEAR(a1);
	return res;
    }

    /* build server_response */
    if (response) {
	str = build_digest(context, a1, NULL);
	if (str == NULL) {
	    MEMSET_FREE_AND_CLEAR(a1);
	    return ENOMEM;
	}

	asprintf(response, "rspauth=%s", str);
	free(str);
    }
    MEMSET_FREE_AND_CLEAR(a1);

    return 0;
}

void
heim_digest_get_session_key(heim_digest_t context, void **key, size_t *keySize)
{
}

void
heim_digest_release(heim_digest_t context)
{
    clear_context(context);
    free(context);
}

