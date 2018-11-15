/*	$NetBSD: digest.c,v 1.1.1.3 2017/01/28 20:46:52 christos Exp $	*/

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
#define F_SERVER	1
#define F_HAVE_HASH	2
#define F_HAVE_HA1	4
#define F_USE_PREFIX	8
    int flags;
    int type;
    char *password;
    uint8_t SecretHash[CC_MD5_DIGEST_LENGTH];
    char *serverNonce;
    char *serverRealm;
    char *serverQOP;
    char *serverMethod;
    char *serverMaxbuf;
    char *serverOpaque;
    char *clientUsername;
    char *clientResponse;
    char *clientURI;
    char *clientRealm;
    char *clientNonce;
    char *clientQOP;
    char *clientNC;
    char *serverAlgorithm;
    char *auth_id;
    
    /* internally allocated objects returned to caller */
    char *serverChallenge;
    char *clientReply;
    char *serverReply;
};

#define FREE_AND_CLEAR(x) do { if ((x)) { free((x)); (x) = NULL; } } while(0)
#define MEMSET_FREE_AND_CLEAR(x) do { if ((x)) { memset(x, 0, strlen(x)); free((x)); (x) = NULL; } } while(0)

static const char digest_prefix[] = "Digest ";

static void
clear_context(heim_digest_t context)
{
    MEMSET_FREE_AND_CLEAR(context->password);
    memset(context->SecretHash, 0, sizeof(context->SecretHash));
    context->flags &= ~(F_HAVE_HASH);
    FREE_AND_CLEAR(context->serverNonce);
    FREE_AND_CLEAR(context->serverRealm);
    FREE_AND_CLEAR(context->serverQOP);
    FREE_AND_CLEAR(context->serverMethod);
    FREE_AND_CLEAR(context->serverMaxbuf);
    FREE_AND_CLEAR(context->serverOpaque);
    FREE_AND_CLEAR(context->clientUsername);
    FREE_AND_CLEAR(context->clientResponse);
    FREE_AND_CLEAR(context->clientURI);
    FREE_AND_CLEAR(context->clientRealm);
    FREE_AND_CLEAR(context->clientNonce);
    FREE_AND_CLEAR(context->clientQOP);
    FREE_AND_CLEAR(context->clientNC);
    FREE_AND_CLEAR(context->serverAlgorithm);
    FREE_AND_CLEAR(context->auth_id);
    
    FREE_AND_CLEAR(context->serverChallenge);
    FREE_AND_CLEAR(context->clientReply);
    FREE_AND_CLEAR(context->serverReply);
}

static void
digest_userhash(const char *user, const char *realm, const char *password,
		unsigned char md[CC_MD5_DIGEST_LENGTH])
{
    CC_MD5_CTX ctx;

    CC_MD5_Init(&ctx);
    CC_MD5_Update(&ctx, user, (CC_LONG)strlen(user));
    CC_MD5_Update(&ctx, ":", 1);
    CC_MD5_Update(&ctx, realm, (CC_LONG)strlen(realm));
    CC_MD5_Update(&ctx, ":", 1);
    CC_MD5_Update(&ctx, password, (CC_LONG)strlen(password));
    CC_MD5_Final(md, &ctx);
}

static char *
build_A1_hash(heim_digest_t context)
{
    unsigned char md[CC_MD5_DIGEST_LENGTH];
    CC_MD5_CTX ctx;
    char *A1;

    if (context->flags & F_HAVE_HA1) {
	memcpy(md, context->SecretHash, sizeof(md));
    } else if (context->flags & F_HAVE_HASH) {
	memcpy(md, context->SecretHash, sizeof(md));
    } else if (context->password) {
	if (context->clientUsername == NULL)
	    return NULL;
	if (context->serverRealm == NULL)
	    return NULL;
	digest_userhash(context->clientUsername,
			context->serverRealm,
			context->password,
			md);
    } else
	return NULL;
    
    if ((context->type == HEIM_DIGEST_TYPE_RFC2617_MD5_SESS || context->type == HEIM_DIGEST_TYPE_RFC2831) && (context->flags & F_HAVE_HA1) == 0) {
	if (context->serverNonce == NULL)
	    return NULL;

	CC_MD5_Init(&ctx);
	CC_MD5_Update(&ctx, md, sizeof(md));
	memset(md, 0, sizeof(md));
	CC_MD5_Update(&ctx, ":", 1);
	CC_MD5_Update(&ctx, context->serverNonce, (CC_LONG)strlen(context->serverNonce));
	if (context->clientNonce) {
	    CC_MD5_Update(&ctx, ":", 1);
	    CC_MD5_Update(&ctx, context->clientNonce, (CC_LONG)strlen(context->clientNonce));
	}
	if (context->type == HEIM_DIGEST_TYPE_RFC2831 && context->auth_id) {
	    CC_MD5_Update(&ctx, ":", 1);
	    CC_MD5_Update(&ctx, context->auth_id, (CC_LONG)strlen(context->auth_id));
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
	CC_MD5_Update(&ctx, method, (CC_LONG)strlen(method));
    CC_MD5_Update(&ctx, ":", 1);
    CC_MD5_Update(&ctx, context->clientURI, (CC_LONG)strlen(context->clientURI));
	
    /* conf|int */
    if (context->type == HEIM_DIGEST_TYPE_RFC2831) {
	if (strcasecmp(context->clientQOP, "auth-int") == 0 || strcasecmp(context->clientQOP, "auth-conf") == 0) {
	    /* XXX if we have a body hash, use that */
	    static char conf_zeros[] = ":00000000000000000000000000000000";
	    CC_MD5_Update(&ctx, conf_zeros, sizeof(conf_zeros) - 1);
	}
    } else {
	/* support auth-int ? */
	if (context->clientQOP && strcasecmp(context->clientQOP, "auth") != 0)
	    return NULL;
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
    struct md5_value *cur;
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

static const char *
check_prefix(heim_digest_t context, const char *challenge)
{
    if (strncasecmp(digest_prefix, challenge, sizeof(digest_prefix) - 1) == 0) {
	
	challenge += sizeof(digest_prefix) - 1;
	while (*challenge == 0x20) /* remove extra space */
	    challenge++;
	context->flags |= F_USE_PREFIX;
    }

    return challenge;
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
    context->flags |= F_SERVER;
    context->type = type;

    return context;
}

static char *
generate_nonce(void)
{
    uint8_t rand[8];
    char *nonce;
    
    if (CCRandomCopyBytes(kCCRandomDefault, rand, sizeof(rand)) != kCCSuccess)
	return NULL;
    
    if (rk_hex_encode(rand, sizeof(rand), &nonce) < 0)
	return NULL;

    return nonce;
}

/**
 * Generate a challange, needs to set serverRealm before calling this function.
 *
 * If type is set to HEIM_DIGEST_TYPE_AUTO, the HEIM_DIGEST_TYPE_RFC2831 will be used instead.
 *
 * For RFC2617 and RFC2831 QOP is required, so if any qop other then "auth" is requested, it need to be set with heim_diest_set_key().
 *
 * @return returns the challenge or NULL on error or failure to build the string. the lifetime
 *         of the string is manage by heim_digest and last until the the context is
 *         freed or until next call to heim_digest_generate_challenge().
 */

const char *
heim_digest_generate_challenge(heim_digest_t context)
{
    char *challenge = NULL;
    
    if (context->serverRealm == NULL)
	return NULL;
    
    if (context->serverNonce == NULL) {
	if ((context->serverNonce = generate_nonce()) == NULL)
	    return NULL;
    }
    
    if (context->serverQOP == NULL) {
	if ((context->serverQOP = strdup("auth")) == NULL)
	    return NULL;
    }
    
    if (context->serverMaxbuf == NULL) {
	if ((context->serverMaxbuf = strdup("65536")) == NULL)
	    return NULL;
    }

    switch(context->type) {
	case HEIM_DIGEST_TYPE_RFC2617_MD5:
	    asprintf(&challenge, "realm=\"%s\",nonce=\"%s\",algorithm=md5,qop=\"%s\"",
		     context->serverRealm, context->serverNonce,
		     context->serverQOP);
	    break;
	case HEIM_DIGEST_TYPE_RFC2617_MD5_SESS:
	    asprintf(&challenge, "realm=\"%s\",nonce=\"%s\",algorithm=md5-sess,qop=\"%s\"",
		     context->serverRealm, context->serverNonce, context->serverQOP);
	    break;
	case HEIM_DIGEST_TYPE_RFC2069:
	    asprintf(&challenge, "realm=\"%s\",nonce=\"%s\"",
		     context->serverRealm, context->serverNonce);
	    break;
	case HEIM_DIGEST_TYPE_AUTO:
	    context->type = HEIM_DIGEST_TYPE_RFC2831;
	    /* FALL THOUGH */
	case HEIM_DIGEST_TYPE_RFC2831:
	    asprintf(&challenge, "realm=\"%s\",nonce=\"%s\",qop=\"%s\",algorithm=md5-sess,charset=utf-8,maxbuf=%s",
		     context->serverRealm, context->serverNonce, context->serverQOP, context->serverMaxbuf);
	    break;
    }

    FREE_AND_CLEAR(context->serverChallenge);
    context->serverChallenge = challenge;
    
    return challenge;
}

int
heim_digest_parse_challenge(heim_digest_t context, const char *challenge)
{
    struct md5_value *val = NULL;
    int ret, type;
    
    challenge = check_prefix(context, challenge);

    ret = parse_values(challenge, &val);
    if (ret)
	goto out;

    ret = 1;

    context->serverNonce = values_find(&val, "nonce");
    if (context->serverNonce == NULL) goto out;

    context->serverRealm = values_find(&val, "realm");
    if (context->serverRealm == NULL) goto out;

    /* check alg */

    context->serverAlgorithm = values_find(&val, "algorithm");
    if (context->serverAlgorithm == NULL || strcasecmp(context->serverAlgorithm, "md5") == 0) {
	type = HEIM_DIGEST_TYPE_RFC2617_MD5;
    } else if (strcasecmp(context->serverAlgorithm, "md5-sess") == 0) {
	type = HEIM_DIGEST_TYPE_RFC2617_OR_RFC2831;
    } else {
	goto out;
    }

    context->serverQOP = values_find(&val, "qop");
    if (context->serverQOP == NULL)
	type = HEIM_DIGEST_TYPE_RFC2069;
    
    context->serverOpaque = values_find(&val, "opaque");

    if (context->type != HEIM_DIGEST_TYPE_AUTO && (context->type & type) == 0)
	goto out;
    else if (context->type == HEIM_DIGEST_TYPE_AUTO)
	context->type = type;

    ret = 0;
 out:
    free_values(val);
    if (ret)
	clear_context(context);
    return ret;
}


static void
set_auth_method(heim_digest_t context)
{
    
    if (context->serverMethod == NULL) {
	if (context->type == HEIM_DIGEST_TYPE_RFC2831)
	    context->serverMethod = strdup("AUTHENTICATE");
	else
	    context->serverMethod = strdup("GET");
    }
}

int
heim_digest_parse_response(heim_digest_t context, const char *response)
{
    struct md5_value *val = NULL;
    char *nonce;
    int ret;

    response = check_prefix(context, response);

    ret = parse_values(response, &val);
    if (ret)
	goto out;

    ret = 1;

    if (context->type == HEIM_DIGEST_TYPE_AUTO) {
	goto out;
    } else if (context->type == HEIM_DIGEST_TYPE_RFC2617_OR_RFC2831) {
	context->clientURI = values_find(&val, "uri");
	if (context->clientURI) {
	    context->type = HEIM_DIGEST_TYPE_RFC2617_MD5_SESS;
	} else {
	    context->clientURI = values_find(&val, "digest-uri");
	    context->type = HEIM_DIGEST_TYPE_RFC2831;
	}
    } else if (context->type == HEIM_DIGEST_TYPE_RFC2831) {
	context->clientURI = values_find(&val, "digest-uri");
    } else {
	context->clientURI = values_find(&val, "uri");
    }

    if (context->clientURI == NULL)
        goto out;

    context->clientUsername = values_find(&val, "username");
    if (context->clientUsername == NULL) goto out;

    /* if client sent realm, make sure its the same of serverRealm if its set */
    context->clientRealm = values_find(&val, "realm");
    if (context->clientRealm && context->serverRealm && strcmp(context->clientRealm, context->serverRealm) != 0)
	goto out;
    
    context->clientResponse = values_find(&val, "response");
    if (context->clientResponse == NULL) goto out;

    nonce = values_find(&val, "nonce");
    if (nonce == NULL) goto out;

    if (strcmp(nonce, context->serverNonce) != 0) {
	free(nonce);
	goto out;
    }
    free(nonce);

    if (context->type != HEIM_DIGEST_TYPE_RFC2069) {

	context->clientQOP = values_find(&val, "qop");
	if (context->clientQOP == NULL) goto out;
	
	/*
	 * If we have serverQOP, lets check that clientQOP exists
	 * in the list of server entries.
	 */
	
	if (context->serverQOP) {
	    Boolean found = false;
	    char *b, *e;
	    size_t len, clen = strlen(context->clientQOP);
	    
	    b = context->serverQOP;
	    while (b && !found) {
		e = strchr(b, ',');
		if (e == NULL)
		    len = strlen(b);
		else {
		    len = e - b;
		    e += 1;
		}
		if (clen == len && strncmp(b, context->clientQOP, len) == 0)
		    found = true;
		b = e;
	    }
	    if (!found)
		goto out;
	}

	context->clientNC = values_find(&val, "nc");
	if (context->clientNC == NULL) goto out;

	context->clientNonce = values_find(&val, "cnonce");
	if (context->clientNonce == NULL) goto out;
    }

    set_auth_method(context);

    ret = 0;
 out:
    free_values(val);
    return ret;
}

char *
heim_digest_userhash(const char *user, const char *realm, const char *password)
{
    unsigned char md[CC_MD5_DIGEST_LENGTH];
    char *str = NULL;

    digest_userhash(user, realm, password, md);

    hex_encode(md, sizeof(md), &str);
    if (str)
      strlwr(str);
    return str;
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
    CC_MD5_Update(&ctx, a1, (CC_LONG)strlen(a1));
    CC_MD5_Update(&ctx, ":", 1);
    CC_MD5_Update(&ctx, context->serverNonce, (CC_LONG)strlen(context->serverNonce));
    if (context->type != HEIM_DIGEST_TYPE_RFC2069) {
	CC_MD5_Update(&ctx, ":", 1);
	CC_MD5_Update(&ctx, context->clientNC, (CC_LONG)strlen(context->clientNC));
	CC_MD5_Update(&ctx, ":", 1);
	CC_MD5_Update(&ctx, context->clientNonce, (CC_LONG)strlen(context->clientNonce));
	CC_MD5_Update(&ctx, ":", 1);
	CC_MD5_Update(&ctx, context->clientQOP, (CC_LONG)strlen(context->clientQOP));
    }
    CC_MD5_Update(&ctx, ":", 1);
    CC_MD5_Update(&ctx, a2, (CC_LONG)strlen(a2));
    CC_MD5_Final(md, &ctx);

    free(a2);

    hex_encode(md, sizeof(md), &str);
    if (str)
      strlwr(str);

    return str;
}

static void
build_server_response(heim_digest_t context, char *a1, char **response)
{
    char *str;
    
    str = build_digest(context, a1, NULL);
    if (str == NULL)
	return;
    
    FREE_AND_CLEAR(context->serverReply);
    asprintf(&context->serverReply, "%srspauth=%s",
	     (context->flags & F_USE_PREFIX) ? digest_prefix : "",
	     str);
    free(str);
    if (response)
	*response = context->serverReply;
}


/**
 * Create response from server to client to server, server verification is in response.
 * clientUsername and clientURI have to be given.
 * If realm is not set, its used from server.
 */

const char *
heim_digest_create_response(heim_digest_t context, char **response)
{
    char *a1, *str, *cnonce = NULL, *opaque = NULL, *uri = NULL, *nc = NULL;
    
    if (response)
	*response = NULL;
    
    if (context->clientUsername == NULL || context->clientURI == NULL)
	return NULL;
    
    if (context->clientRealm == NULL) {
	if (context->serverRealm == NULL)
	    return NULL;
	if ((context->clientRealm = strdup(context->serverRealm)) == NULL)
	    return NULL;
    }
    
    if (context->type != HEIM_DIGEST_TYPE_RFC2069) {
	if (context->clientNC == NULL) {
	    if ((context->clientNC = strdup("00000001")) == NULL)
		return NULL;
	}
	if (context->clientNonce == NULL) {
	    if ((context->clientNonce = generate_nonce()) == NULL)
		return NULL;
	}

	/**
	 * If using non RFC2069, appropriate QOP should be set.
	 *
	 * Pick QOP from server if not given, if its a list, pick the first entry
	 */
	if (context->clientQOP == NULL) {
	    char *r;
	    if (context->serverQOP == NULL)
		return NULL;
	    r = strchr(context->serverQOP, ',');
	    if (r == NULL) {
		if ((context->clientQOP = strdup(context->serverQOP)) == NULL)
			return NULL;
	    } else {
		size_t len = (r - context->serverQOP) + 1;
		if ((context->clientQOP = malloc(len)) == NULL)
		    return NULL;
		strlcpy(context->clientQOP, context->serverQOP, len);
	    }
	}
    }
	    
    set_auth_method(context);
    
    a1 = build_A1_hash(context);
    if (a1 == NULL)
	return NULL;
    
    str = build_digest(context, a1, context->serverMethod);
    if (str == NULL) {
	MEMSET_FREE_AND_CLEAR(a1);
	return NULL;
    }
    
    MEMSET_FREE_AND_CLEAR(context->clientResponse);
    context->clientResponse = str;
    
    if (context->clientURI) {
	const char *name = "digest-uri";
	if (context->type != HEIM_DIGEST_TYPE_RFC2831)
	    name = "uri";
	asprintf(&uri, ",%s=\"%s\"", name, context->clientURI);
    }
    
    if (context->serverOpaque)
	asprintf(&opaque, ",opaque=\"%s\"", context->serverOpaque);
    
    if (context->clientNonce)
	asprintf(&cnonce, ",cnonce=\"%s\"", context->clientNonce);

    if (context->clientNC)
	asprintf(&nc, ",nc=%s", context->clientNC);
    
    asprintf(&context->clientReply,
	     "username=%s,realm=%s,nonce=\"%s\",qop=\"%s\"%s%s%s,response=\"%s\"%s",
	     context->clientUsername, context->clientRealm,
	     context->serverNonce,
	     context->clientQOP,
	     uri ? uri : "",
	     cnonce ? cnonce : "",
	     nc ? nc : "",
	     context->clientResponse,
	     opaque ? opaque : "");
    
    build_server_response(context, a1, response);
    MEMSET_FREE_AND_CLEAR(a1);
    FREE_AND_CLEAR(uri);
    FREE_AND_CLEAR(opaque);
    FREE_AND_CLEAR(cnonce);
    FREE_AND_CLEAR(nc);
    
    return context->clientReply;
}

int
heim_digest_verify(heim_digest_t context, char **response)
{
    char *a1;
    char *str;
    int res;

    if (response)
	*response = NULL;
    
    set_auth_method(context);

    a1 = build_A1_hash(context);
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
    build_server_response(context, a1, response);
    MEMSET_FREE_AND_CLEAR(a1);
    /* XXX break ABI and return internally allocated string instead */
    if (response)
	*response = strdup(*response);

    return 0;
}

/**
 * Create a rspauth= response.
 * Assumes that the A1hash/password serverNonce, clientNC, clientNonce, clientQOP is set.
 *
 * @return the rspauth string (including rspauth), return key are stored in serverReply and will be invalid after another call to heim_digest_*
 */

const char *
heim_digest_server_response(heim_digest_t context)
{
    char *a1;
    
    if (context->serverNonce == NULL)
	return NULL;
    if (context->clientURI == NULL)
	return NULL;

    a1 = build_A1_hash(context);
    if (a1 == NULL)
	return NULL;
    
    build_server_response(context, a1, NULL);
    MEMSET_FREE_AND_CLEAR(a1);
    
    return context->serverReply;
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

struct {
    char *name;
    size_t offset;
} keys[] = {
#define KVN(value) { #value, offsetof(struct heim_digest_desc, value) }
    KVN(serverNonce),
    KVN(serverRealm),
    KVN(serverQOP),
    KVN(serverMethod),
    { "method", offsetof(struct heim_digest_desc, serverMethod) },
    KVN(serverMaxbuf),
    KVN(clientUsername),
    { "username", offsetof(struct heim_digest_desc, clientUsername) },
    KVN(clientResponse),
    KVN(clientURI),
    { "uri", offsetof(struct heim_digest_desc, clientURI) },
    KVN(clientRealm),
    { "realm", offsetof(struct heim_digest_desc, clientRealm) },
    KVN(clientNonce),
    KVN(clientQOP),
    KVN(clientNC),
    KVN(serverAlgorithm),
    KVN(auth_id)
#undef KVN
};

const char *
heim_digest_get_key(heim_digest_t context, const char *key)
{
    size_t n;

    for (n = 0; n < sizeof(keys) / sizeof(keys[0]); n++) {
	if (strcasecmp(key, keys[n].name) == 0) {
	    char **ptr = (char **)((((char *)context) + keys[n].offset));
	    return *ptr;
	}
    }
    return NULL;
}

int
heim_digest_set_key(heim_digest_t context, const char *key, const char *value)
{

    if (strcmp(key, "password") == 0) {
	FREE_AND_CLEAR(context->password);
	if ((context->password = strdup(value)) == NULL)
	    return ENOMEM;
	context->flags &= ~(F_HAVE_HASH|F_HAVE_HA1);
    } else if (strcmp(key, "userhash") == 0) {
	ssize_t ret;
	FREE_AND_CLEAR(context->password);

	ret = hex_decode(value, context->SecretHash, sizeof(context->SecretHash));
	if (ret != sizeof(context->SecretHash))
	    return EINVAL;
	context->flags &= ~F_HAVE_HA1;
	context->flags |= F_HAVE_HASH;
    } else if (strcmp(key, "H(A1)") == 0) {
	ssize_t ret;
	FREE_AND_CLEAR(context->password);
	
	ret = hex_decode(value, context->SecretHash, sizeof(context->SecretHash));
	if (ret != sizeof(context->SecretHash))
	    return EINVAL;
	context->flags &= ~F_HAVE_HASH;
	context->flags |= F_HAVE_HA1;
    } else if (strcmp(key, "method") == 0) {
	FREE_AND_CLEAR(context->serverMethod);
	if ((context->serverMethod = strdup(value)) == NULL)
	    return ENOMEM;
    } else {
	size_t n;

	for (n = 0; n < sizeof(keys) / sizeof(keys[0]); n++) {
	    if (strcasecmp(key, keys[n].name) == 0) {
		char **ptr = (char **)((((char *)context) + keys[n].offset));
		FREE_AND_CLEAR(*ptr);
		if (((*ptr) = strdup(value)) == NULL)
		    return ENOMEM;
		break;
	    }
	}
	if (n == sizeof(keys) / sizeof(keys[0]))
	    return ENOENT;
    }
    return 0;
}

