/* $NetBSD: mech_digestmd5.c,v 1.11 2013/06/28 15:04:35 joerg Exp $ */

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
 *        This product includes software developed by the NetBSD
 *        Foundation, Inc. and its contributors.
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
__RCSID("$NetBSD: mech_digestmd5.c,v 1.11 2013/06/28 15:04:35 joerg Exp $");

#include <sys/param.h>

#include <assert.h>
#include <ctype.h>
#include <md5.h>
#include <saslc.h>
#include <stdbool.h>
#include <stdio.h>
#include <string.h>

#include <openssl/evp.h>

#include "buffer.h"
#include "crypto.h"
#include "error.h"
#include "list.h"
#include "mech.h"
#include "msg.h"
#include "saslc_private.h"

/* See RFC 2831. */

/*
 * TODO:
 *
 * 1) Add support for Subsequent Authentication (see RFC 2831 section 2.2).
 */

/* properties */
#define SASLC_DIGESTMD5_AUTHCID		SASLC_PROP_AUTHCID
#define SASLC_DIGESTMD5_AUTHZID		SASLC_PROP_AUTHZID
#define SASLC_DIGESTMD5_CIPHERMASK	SASLC_PROP_CIPHERMASK
#define SASLC_DIGESTMD5_HOSTNAME	SASLC_PROP_HOSTNAME
#define SASLC_DIGESTMD5_MAXBUF		SASLC_PROP_MAXBUF
#define SASLC_DIGESTMD5_PASSWD		SASLC_PROP_PASSWD
#define SASLC_DIGESTMD5_QOPMASK		SASLC_PROP_QOPMASK
#define SASLC_DIGESTMD5_REALM		SASLC_PROP_REALM
#define SASLC_DIGESTMD5_SERVICE		SASLC_PROP_SERVICE
#define SASLC_DIGESTMD5_SERVNAME	SASLC_PROP_SERVNAME
/*
 * XXX: define this if you want to be able to set a fixed cnonce for
 * debugging purposes.
 */
#define SASLC_DIGESTMD5_CNONCE		"CNONCE"
/*
 * XXX: define this if you want to test the saslc_sess_encode() and
 * saslc_sess_decode() routines against themselves, i.e., have them
 * use the same key.
 */
#define SASLC_DIGESTMD5_SELFTEST	"SELFTEST"

#define DEFAULT_QOP_MASK	(F_QOP_NONE | F_QOP_INT | F_QOP_CONF)
#define DEFAULT_CIPHER_MASK	(F_CIPHER_DES | F_CIPHER_3DES | \
				 F_CIPHER_RC4 | F_CIPHER_RC4_40 | \
				 F_CIPHER_RC4_56 | F_CIPHER_AES)

#define DEFAULT_MAXBUF		0x10000
#define MAX_MAXBUF		0xffffff
#define INVALID_MAXBUF(m)	((m) <= sizeof(md5hash_t) && (m) > MAX_MAXBUF)

#define NONCE_LEN 33	/* Minimum recommended length is 64bits (rfc2831).
			   cyrus-sasl uses 33 bytes. */

typedef enum {
	CHALLENGE_IGNORE	= -1,	/* must be -1 */
	CHALLENGE_REALM		= 0,
	CHALLENGE_NONCE		= 1,
	CHALLENGE_QOP		= 2,
	CHALLENGE_STALE		= 3,
	CHALLENGE_MAXBUF	= 4,
	CHALLENGE_CHARSET	= 5,
	CHALLENGE_ALGORITHM	= 6,
	CHALLENGE_CIPHER	= 7
} challenge_t;

typedef enum {
	/*
	 * NB: Values used to index cipher_tbl[] and cipher_ctx_tbl[]
	 *     in cipher_context_create().
	 */
	CIPHER_DES	= 0,
	CIPHER_3DES	= 1,
	CIPHER_RC4	= 2,
	CIPHER_RC4_40	= 3,
	CIPHER_RC4_56	= 4,
	CIPHER_AES	= 5
} cipher_t;

#define F_CIPHER_DES		(1 << CIPHER_DES)
#define F_CIPHER_3DES		(1 << CIPHER_3DES)
#define F_CIPHER_RC4		(1 << CIPHER_RC4)
#define F_CIPHER_RC4_40		(1 << CIPHER_RC4_40)
#define F_CIPHER_RC4_56		(1 << CIPHER_RC4_56)
#define F_CIPHER_AES		(1 << CIPHER_AES)

static const named_flag_t cipher_tbl[] = {
	/* NB: to be indexed by cipher_t values */
	{ "des",	F_CIPHER_DES    },
	{ "3des",	F_CIPHER_3DES   },
	{ "rc4",	F_CIPHER_RC4    },
	{ "rc4-40",	F_CIPHER_RC4_40 },
	{ "rc4-56",	F_CIPHER_RC4_56 },
	{ "aes",	F_CIPHER_AES    },
	{ NULL,		0               }
};

static inline const char *
cipher_name(cipher_t cipher)
{

	assert(cipher < __arraycount(cipher_tbl) - 1); /* NULL terminated */
	if (cipher < __arraycount(cipher_tbl) - 1)
		return cipher_tbl[cipher].name;
	return NULL;
}

static inline unsigned int
cipher_list_flags(list_t *list)
{

	return saslc__list_flags(list, cipher_tbl);
}

typedef struct { /* data parsed from challenge */
	bool		utf8;
	bool	 	algorithm;
	bool	 	stale;
	char *		nonce;
	list_t *	realm;
	uint32_t	cipher_flags;
	uint32_t	qop_flags;
	size_t		maxbuf;
} cdata_t;

typedef struct { /* response data */
	/* NB: the qop is in saslc__mech_sess_t */
	char *authcid;
	char *authzid;
	char *cnonce;
	char *digesturi;
	char *passwd;
	char *realm;
	cipher_t cipher;
	int nonce_cnt;
	size_t maxbuf;
} rdata_t;

typedef uint8_t md5hash_t[MD5_DIGEST_LENGTH];

typedef struct {
	md5hash_t kic;			/* client->server integrity key */
	md5hash_t kis;			/* server->client integrity key */
	md5hash_t kcc;			/* client->server confidentiality key */
	md5hash_t kcs;			/* server->client confidentiality key */
} keys_t;

typedef struct cipher_context_t {
	size_t blksize;			/* block size for cipher */
	EVP_CIPHER_CTX *evp_ctx;	/* openssl EVP context */
} cipher_context_t;

typedef struct coder_context_t {
	uint8_t *key;			/* key for coding */
	uint32_t seqnum;		/* 4 byte sequence number */

	void *buf_ctx;			/* buffer context */
	cipher_context_t *cph_ctx;	/* cipher context */
	saslc_sess_t *sess;		/* session: for error setting */
} coder_context_t;

/* mech state */
typedef struct {
	saslc__mech_sess_t mech_sess;	/* must be first */
	cdata_t cdata;			/* data parsed from challenge string */
	rdata_t rdata;			/* data used for response string */
	keys_t keys;			/* keys */
	coder_context_t dec_ctx;	/* decode context */
	coder_context_t enc_ctx;	/* encode context */
} saslc__mech_digestmd5_sess_t;

/**
 * @brief if possible convert a UTF-8 string to a ISO8859-1 string.
 * @param utf8 original UTF-8 string.
 * @param iso8859 pointer to pointer to the malloced ISO8859-1 string.
 * @return -1 if the string cannot be translated.
 *
 * NOTE: this allocates memory for its output and the caller is
 * responsible for freeing it.
 */
static int
utf8_to_8859_1(char *utf8, char **iso8859)
{
	unsigned char *s, *d, *end, *src;
	size_t cnt;

	src = (unsigned char *)utf8;
	cnt = 0;
	end = src + strlen(utf8);
	for (s = src; s < end; ++s) {
		if (*s > 0xC3) /* abort if outside 8859-1 range */
			return -1;
		/*
		 * Look for valid 2 byte UTF-8 encoding with, 8 bits
		 * of info.  Quit if invalid pair found.
		 */
		if (*s >= 0xC0 && *s <= 0xC3) { /* 2 bytes, 8 bits */
			if (++s == end || *s < 0x80 || *s > 0xBF)
				return -1;	/* broken utf-8 encoding */
		}
		cnt++;
	}

	/* Allocate adequate space. */
	d = malloc(cnt + 1);
	if (d == NULL)
		return -1;

	*iso8859 = (char *)d;

	/* convert to 8859-1 */
	do {
		for (s = src; s < end && *s < 0xC0; ++s)
			*d++ = *s;
		if (s + 1 >= end)
			break;
		*d++ = ((s[0] & 0x3) << 6) | (s[1] & 0x3f);
		src = s + 2;
	} while (src < end);

	*d = '\0';
	return 0;
}

/**
 * @brief unquote a string by removing escapes.
 * @param str string to unquote.
 * @return NULL on failure
 *
 * NOTE: this allocates memory for its output and the caller is
 * responsible for freeing it.
 */
static char *
unq(const char *str)
{
	const char *s;
	char *unq_str, *d;
	int escaped;

	unq_str = malloc(strlen(str) + 1);
	if (unq_str == NULL)
		return NULL;

	escaped = 0;
	d = unq_str;
	for (s = str; *s != '\0'; s++) {
		switch (*s) {
		case '\\':
			if (escaped)
				*d++ = *s;
			escaped = !escaped;
			break;
		default:
			*d++ = *s;
			escaped = 0;
		}
	}
	*d = '\0';

	return unq_str;
}

/**
 * @brief computing MD5(username:realm:password).
 * @param ms mechanism session
 * @param buf buffer for hash
 * @return 0 on success, -1 on failure
 */
static int
saslc__mech_digestmd5_userhash(saslc__mech_digestmd5_sess_t *ms, uint8_t *buf)
{
	char *tmp;
	char *unq_username, *unq_realm;
	ssize_t len;

	if ((unq_username = unq(ms->rdata.authcid)) == NULL)
		return -1;

	/********************************************************/
	/* RFC 2831 section 2.1.2				*/
	/* ...  If the directive is missing, "realm-value" will */
	/* set to the empty string when computing A1.	  	*/
	/********************************************************/
	if (ms->rdata.realm == NULL)
		unq_realm = strdup("");
	else
		unq_realm = unq(ms->rdata.realm);

	if (unq_realm == NULL) {
		free(unq_username);
		return -1;
	}
	len = asprintf(&tmp, "%s:%s:%s",
			unq_username, unq_realm, ms->rdata.passwd);
	free(unq_realm);
	free(unq_username);

	if (len == -1)
		return -1;

	saslc__crypto_md5_hash(tmp, (size_t)len, buf);
	memset(tmp, 0, (size_t)len);
	free(tmp);
	return 0;
}

/**
 * @brief setup the appropriate QOP keys as determined by the chosen
 * QOP type (see RFC2831 sections 2.3 and 2.4).
 * @param ms mechanism session
 * @param a1hash MD5(a1)
 * @return 0 on success, -1 on failure
 */
static int
setup_qop_keys(saslc__mech_digestmd5_sess_t *ms, md5hash_t a1hash)
{
#define KIC_MAGIC "Digest session key to client-to-server signing key magic constant"
#define KIS_MAGIC "Digest session key to server-to-client signing key magic constant"
#define KCC_MAGIC "Digest H(A1) to client-to-server sealing key magic constant"
#define KCS_MAGIC "Digest H(A1) to server-to-client sealing key magic constant"
#define KIC_MAGIC_LEN (sizeof(KIC_MAGIC) - 1)
#define KIS_MAGIC_LEN (sizeof(KIS_MAGIC) - 1)
#define KCC_MAGIC_LEN (sizeof(KCC_MAGIC) - 1)
#define KCS_MAGIC_LEN (sizeof(KCS_MAGIC) - 1)
#define MAX_MAGIC_LEN KIC_MAGIC_LEN

	char buf[MD5_DIGEST_LENGTH + MAX_MAGIC_LEN];
	size_t buflen;
	size_t n;

	switch (ms->mech_sess.qop) {
	case QOP_NONE:
		/* nothing to do */
		break;

	case QOP_CONF:
    /*************************************************************************/
    /* See RFC2831 section 2.4 (Confidentiality Protection)                  */
    /*                                                                       */
    /* The key for confidentiality protecting messages from client to server */
    /* is:                                                                   */
    /*                                                                       */
    /* Kcc = MD5({H(A1)[0..n],                                               */
    /* "Digest H(A1) to client-to-server sealing key magic constant"})       */
    /*                                                                       */
    /* The key for confidentiality protecting messages from server to client */
    /* is:                                                                   */
    /*                                                                       */
    /* Kcs = MD5({H(A1)[0..n],                                               */
    /* "Digest H(A1) to server-to-client sealing key magic constant"})       */
    /*                                                                       */
    /* where MD5 is as specified in [RFC 1321]. For cipher "rc4-40" n is 5;  */
    /* for "rc4-56" n is 7; for the rest n is 16.                            */
    /*************************************************************************/

		switch (ms->rdata.cipher) {
		case CIPHER_RC4_40:	n = 5;			break;
		case CIPHER_RC4_56:	n = 7;			break;
		default:		n = MD5_DIGEST_LENGTH;	break;
		}
		memcpy(buf, a1hash, n);

		memcpy(buf + n, KCC_MAGIC, KCC_MAGIC_LEN);
		buflen = n + KCC_MAGIC_LEN;
		saslc__crypto_md5_hash(buf, buflen, ms->keys.kcc);

		memcpy(buf + n, KCS_MAGIC, KCS_MAGIC_LEN);
		buflen = n + KCS_MAGIC_LEN;
		saslc__crypto_md5_hash(buf, buflen, ms->keys.kcs);

		/*FALLTHROUGH*/

	case QOP_INT:
    /*************************************************************************/
    /* See RFC2831 section 2.3 (Integrity Protection)                        */
    /* The key for integrity protecting messages from client to server is:   */
    /*                                                                       */
    /* Kic = MD5({H(A1),                                                     */
    /* "Digest session key to client-to-server signing key magic constant"}) */
    /*                                                                       */
    /* The key for integrity protecting messages from server to client is:   */
    /*                                                                       */
    /* Kis = MD5({H(A1),                                                     */
    /* "Digest session key to server-to-client signing key magic constant"}) */
    /*************************************************************************/
		memcpy(buf, a1hash, MD5_DIGEST_LENGTH);

		memcpy(buf + MD5_DIGEST_LENGTH, KIC_MAGIC, KIC_MAGIC_LEN);
		buflen = MD5_DIGEST_LENGTH + KIC_MAGIC_LEN;
		saslc__crypto_md5_hash(buf, buflen, ms->keys.kic);

		memcpy(buf + MD5_DIGEST_LENGTH, KIS_MAGIC, KIS_MAGIC_LEN);
		buflen = MD5_DIGEST_LENGTH + KIS_MAGIC_LEN;
		saslc__crypto_md5_hash(buf, buflen, ms->keys.kis);
		break;
	}
	return 0;

#undef KIC_MAGIC
#undef KIS_MAGIC
#undef KCC_MAGIC
#undef KCS_MAGIC
#undef KIC_MAGIC_LEN
#undef KIS_MAGIC_LEN
#undef KCC_MAGIC_LEN
#undef KCS_MAGIC_LEN
#undef MAX_MAGIC_LEN
}

/**
 * @brief computes A1 hash value (see: RFC2831)
 * @param ms mechanism session
 * @return hash in hex form
 */
static char *
saslc__mech_digestmd5_a1(saslc__mech_digestmd5_sess_t *ms)
{
	char *tmp1, *tmp2, *r;
	char *unq_authzid;
	md5hash_t a1hash, userhash;
	int plen;
	size_t len;
 /*****************************************************************************/
 /* If authzid is specified, then A1 is                                       */
 /*                                                                           */
 /*    A1 = { H({ unq(username-value), ":", unq(realm-value), ":", passwd }), */
 /*         ":", nonce-value, ":", cnonce-value, ":", unq(authzid-value) }    */
 /*                                                                           */
 /* If authzid is not specified, then A1 is                                   */
 /*                                                                           */
 /*    A1 = { H({ unq(username-value), ":", unq(realm-value), ":", passwd }), */
 /*         ":", nonce-value, ":", cnonce-value }                             */
 /*****************************************************************************/

	if (saslc__mech_digestmd5_userhash(ms, userhash) == -1)
		return NULL;

	if (ms->rdata.authzid == NULL)
		plen = asprintf(&tmp1, ":%s:%s",
		    ms->cdata.nonce, ms->rdata.cnonce);
	else {
		if ((unq_authzid = unq(ms->rdata.authzid)) == NULL)
			return NULL;

		plen = asprintf(&tmp1, ":%s:%s:%s",
		    ms->cdata.nonce, ms->rdata.cnonce, unq_authzid);
		free(unq_authzid);
	}
	if (plen == -1)
		return NULL;
	len = plen;

	tmp2 = malloc(MD5_DIGEST_LENGTH + len);
	if (tmp2 == NULL) {
		free(tmp1);
		return NULL;
	}
	memcpy(tmp2, userhash, MD5_DIGEST_LENGTH);
	memcpy(tmp2 + MD5_DIGEST_LENGTH, tmp1, len);
	free(tmp1);

	saslc__crypto_md5_hash(tmp2, MD5_DIGEST_LENGTH + len, a1hash);
	free(tmp2);

	r = saslc__crypto_hash_to_hex(a1hash);
	setup_qop_keys(ms, a1hash);
	return r;
}

/**
 * @brief computes A2 hash value (see: RFC2831)
 * @param ms mechanism session
 * @param method string indicating method "AUTHENTICATE" or ""
 * @return hash converted to ascii
 */
static char *
saslc__mech_digestmd5_a2(saslc__mech_digestmd5_sess_t *ms,
    const char *method)
{
	char *tmp, *r;
	int rval;
	/*****************************************************************/
	/* If the "qop" directive's value is "auth", then A2 is:         */
	/*                                                               */
	/*    A2       = { "AUTHENTICATE:", digest-uri-value }           */
	/*                                                               */
	/* If the "qop" value is "auth-int" or "auth-conf" then A2 is:   */
	/*                                                               */
	/*    A2       = { "AUTHENTICATE:", digest-uri-value,            */
	/*               ":00000000000000000000000000000000" }           */
	/*****************************************************************/

	rval = -1;
	switch(ms->mech_sess.qop) {
	case QOP_NONE:
		rval = asprintf(&tmp, "%s:%s", method,
		    ms->rdata.digesturi);
		break;
	case QOP_INT:
	case QOP_CONF:
		rval = asprintf(&tmp,
		    "%s:%s:00000000000000000000000000000000",
		    method, ms->rdata.digesturi);
		break;
	}
	if (rval == -1)
		return NULL;

	r = saslc__crypto_md5_hex(tmp, strlen(tmp));
	free(tmp);
	return r;
}

/**
 * @brief computes result hash.
 * @param ms mechanism session
 * @param a1 A1 hash value
 * @param a2 A2 hash value
 * @return hash converted to ascii, NULL on failure.
 */
static char *
saslc__mech_digestmd5_rhash(saslc__mech_digestmd5_sess_t *ms,
    const char *a1, const char *a2)
{
	char *tmp, *r;
	/******************************************************************/
	/* response-value  =                                              */
	/*    HEX( KD ( HEX(H(A1)),                                       */
	/*            { nonce-value, ":" nc-value, ":",                   */
	/*              cnonce-value, ":", qop-value, ":", HEX(H(A2)) })) */
	/******************************************************************/

	if (asprintf(&tmp, "%s:%s:%08x:%s:%s:%s", a1, ms->cdata.nonce,
		ms->rdata.nonce_cnt, ms->rdata.cnonce,
		saslc__mech_qop_name(ms->mech_sess.qop), a2)
	    == -1)
		return NULL;

	r = saslc__crypto_md5_hex(tmp, strlen(tmp));
	free(tmp);
	return r;
}

/**
 * @brief building response string. Basing on
 * session and mechanism properties.
 * @param ms mechanism session
 * @param method string indicating method: "AUTHENTICATE" or ""
 * @return response string, NULL on failure.
 */
static char *
saslc__mech_digestmd5_response(saslc__mech_digestmd5_sess_t *ms,
    const char *method)
{
	char *r, *a1, *a2;

	/******************************************************************/
	/* charset = "charset" "=" "utf-8"                                */
	/*                                                                */
	/* This directive, if present, specifies that the client has used */
	/* UTF-8 [UTF-8] encoding for the username, realm and             */
	/* password. If present, the username, realm and password are in  */
	/* Unicode, prepared using the "SASLPrep" profile [SASLPrep] of   */
	/* the "stringprep" algorithm [StringPrep] and than encoded as    */
	/* UTF-8 [UTF-8].  If not present, the username and password must */
	/* be encoded in ISO 8859-1 [ISO-8859] (of which US-ASCII         */
	/* [USASCII] is a subset). The client should send this directive  */
	/* only if the server has indicated it supports UTF-8             */
	/* [UTF-8]. The directive is needed for backwards compatibility   */
	/* with HTTP Digest, which only supports ISO 8859-1.              */
	/******************************************************************/
	/*
	 * NOTE: We don't set charset in the response, so this is not
	 * an issue here.  However, see the note in stringprep_realms()
	 * which is called when processing the challenge.
	 */
	/******************************************************************/
	/* response-value  =                                              */
	/*    HEX( KD ( HEX(H(A1)),                                       */
	/*            { nonce-value, ":" nc-value, ":",                   */
	/*              cnonce-value, ":", qop-value, ":", HEX(H(A2)) })) */
	/******************************************************************/

	r = NULL;

	a1 = saslc__mech_digestmd5_a1(ms);
	if (a1 == NULL)
		return NULL;

	a2 = saslc__mech_digestmd5_a2(ms, method);
	if (a2 != NULL) {
		r = saslc__mech_digestmd5_rhash(ms, a1, a2);
		free(a2);
	}
	free(a1);
	return r;
}

/**
 * @brief Choose a string from a user provided host qualified list,
 * i.e., a comma delimited list with possible hostname qualifiers on
 * the elements.
 * @param hqlist a comma delimited list with entries of the form
 * "[hostname:]string".
 * @param hostname the hostname to use in the selection.
 * @param rval pointer to location for returned string.  Set to NULL
 * if none found, otherwise set to strdup(3) of the string found.
 * @return 0 on success, -1 on failure (no memory).
 *
 * NOTE: hqlist and rval must not be NULL.
 * NOTE: this allocates memory for its output and the caller is
 * responsible for freeing it.
 */
static int
choose_from_hqlist(const char *hqlist, const char *hostname, char **rval)
{
	list_t *l, *list;
	size_t len;
	char *p;

	if (saslc__list_parse(&list, hqlist) == -1)
		return -1;	/* no memory */

	/*
	 * If the user provided a list and the caller provided a
	 * hostname, pick the first string from the list that
	 * corresponds to the hostname.
	 */
	if (hostname != NULL) {
		len = strlen(hostname);
		for (l = list; l != NULL; l = l->next) {
			p = l->value + len;
			if (*p != ':' ||
			    strncasecmp(l->value, hostname, len) != 0)
				continue;

			if (*(++p) != '\0' && isalnum((unsigned char)*p)) {
				if ((p = strdup(p)) == NULL)
					goto nomem;
				goto done;
			}
		}
	}
	/*
	 * If one couldn't be found, look for first string in the list
	 * without a hostname specifier.
	 */
	p = NULL;
	for (l = list; l != NULL; l = l->next) {
		if (strchr(l->value, ':') == NULL) {
			if ((p = strdup(l->value)) == NULL)
				goto nomem;
			goto done;
		}
	}
 done:
	saslc__list_free(list);
	*rval = p;
	return 0;
 nomem:
	saslc__list_free(list);
	return -1;
}

/**
 * @brief builds digesturi string
 * @param serv_type type of service to use, e.g., "smtp"
 * @param host fully-qualified canonical DNS name of host
 * @param serv_name service name if it is replicated via DNS records; may
 * be NULL.
 * @return digesturi string, NULL on failure.
 */
static char *
saslc__mech_digestmd5_digesturi(saslc_sess_t *sess, const char *serv_host)
{
	const char *serv_list;
	char *serv_name;
	const char *serv_type;
	char *r;
	int rv;

	serv_type = saslc_sess_getprop(sess, SASLC_DIGESTMD5_SERVICE);
	if (serv_type == NULL) {
		saslc__error_set(ERR(sess), ERROR_MECH,
		    "service is required for an authentication");
		return NULL;
	}
	serv_list = saslc_sess_getprop(sess, SASLC_DIGESTMD5_SERVNAME);
	if (serv_list == NULL)
		serv_name = NULL;
	else if (choose_from_hqlist(serv_list, serv_host, &serv_name) == -1)
		goto nomem;

	saslc__msg_dbg("%s: serv_name='%s'", __func__,
	    serv_name ? serv_name : "<null>");

	/****************************************************************/
	/* digest-uri       = "digest-uri" "=" <"> digest-uri-value <">	*/
	/* digest-uri-value  = serv-type "/" host [ "/" serv-name ]	*/
	/*								*/
	/* If the service is not replicated, or the serv-name is	*/
	/* identical to the host, then the serv-name component MUST be	*/
	/* omitted.  The service is considered to be replicated if the	*/
	/* client's service-location process involves resolution using	*/
	/* standard DNS lookup operations, and if these operations	*/
	/* involve DNS records (such as SRV, or MX) which resolve one	*/
	/* DNS name into a set of other DNS names.			*/
	/****************************************************************/

	rv = serv_name == NULL || strcmp(serv_host, serv_name) == 0
	    ? asprintf(&r, "%s/%s", serv_type, serv_host)
	    : asprintf(&r, "%s/%s/%s", serv_type, serv_host, serv_name);
	if (serv_name != NULL)
		free(serv_name);
	if (rv == -1)
		goto nomem;

	saslc__msg_dbg("%s: digest-uri='%s'", __func__, r);
	return r;
 nomem:
	saslc__error_set_errno(ERR(sess), ERROR_NOMEM);
	return NULL;
}

/**
 * @brief creates client's nonce. (Basing on crypto.h)
 * @param s length of nonce
 * @return nonce string, NULL on failure.
 */
static char *
saslc__mech_digestmd5_nonce(size_t s)
{
	char *nonce;
	char *r;

	nonce = saslc__crypto_nonce(s);
	if (nonce == NULL)
		return NULL;

	if (saslc__crypto_encode_base64(nonce, s, &r, NULL) == -1)
		return NULL;
	free(nonce);

	return r;
}

/**
 * @brief strip quotes from a string (modifies the string)
 * @param str the string
 * @return string without quotes.
 */
static char *
strip_quotes(char *str)
{
	char *p;
	size_t len;

	if (*str != '"')
		return str;

	len = strlen(str);
	p = str + len;
	if (len < 2 || p[-1] != '"')
		return str;

	p[-1] = '\0';
	return ++str;
}

/**
 * @brief convert a list of realms from utf-8 to iso8859-q if necessary.
 * @param is_utf8 the characterset of the realms (true if utf8)
 * @param realms the realm list
 */
static int
stringprep_realms(bool is_utf8, list_t *realms)
{
	list_t *l;
	char *utf8, *iso8859;

	/******************************************************************/
	/* If at least one realm is present and the charset directive is  */
	/* also specified (which means that realm(s) are encoded as       */
	/* UTF-8), the client should prepare each instance of realm using */
	/* the "SASLPrep" profile [SASLPrep] of the "stringprep"          */
	/* algorithm [StringPrep]. If preparation of a realm instance     */
	/* fails or results in an empty string, the client should abort   */
	/* the authentication exchange.                                   */
	/******************************************************************/
	if (!is_utf8)
		return 0;

	for (l = realms; l != NULL; l = l->next) {
		utf8 = l->value;
		if (utf8_to_8859_1(utf8, &iso8859) == -1)
			return -1;
		free(utf8);
		l->value = iso8859;
	}
	return 0;
}

/**
 * @brief choose a realm from a list of possible realms provided by the server
 * @param sess the session context
 * @param realms the list of realms
 * @return our choice of realm or NULL on failure.  It is the user's
 * responsibility to free the memory allocated for the return string.
 */
static char *
choose_realm(saslc_sess_t *sess, const char *hostname, list_t *realms)
{
	const char *user_realms;
	list_t *l;
	char *p;

	/*****************************************************************/
	/* The realm containing the user's account. This directive is	 */
	/* required if the server provided any realms in the		 */
	/* "digest-challenge", in which case it may appear exactly once  */
	/* and its value SHOULD be one of those realms. If the directive */
	/* is missing, "realm-value" will set to the empty string when	 */
	/* computing A1 (see below for details).			 */
	/*****************************************************************/

	user_realms = saslc_sess_getprop(sess, SASLC_DIGESTMD5_REALM);

	/*
	 * If the challenge provided no realms, try to pick one from a
	 * user specified list, which may be keyed by the hostname.
	 * If one can't be found, return NULL;
	 */
	if (realms == NULL) {
		/*
		 * No realm was supplied in challenge.  Figure out a
		 * plausable default.
		 */
		if (user_realms == NULL) {
			saslc__error_set(ERR(sess), ERROR_MECH,
			    "cannot determine the realm");
			return NULL;
		}
		if (choose_from_hqlist(user_realms, hostname, &p) == -1)
			goto nomem;

		if (p == NULL)
			saslc__error_set(ERR(sess), ERROR_MECH,
			    "cannot choose a realm");
		return p;
	}

	/************************************************************/
	/* Multiple realm directives are allowed, in which case the */
	/* user or client must choose one as the realm for which to */
	/* supply to username and password.                         */
	/************************************************************/
	/*
	 * If the user hasn't specified any realms, or we can't find
	 * one from the user provided list, just take the first realm
	 * from the challenge.
	 */
	if (user_realms == NULL)
		goto use_1st_realm;

	if (choose_from_hqlist(user_realms, hostname, &p) == -1)
		goto nomem;

	if (p == NULL)
		goto use_1st_realm;

	/*
	 * If we found a matching user provide realm, make sure it is
	 * on the list of realms.  If it isn't, just take the first
	 * realm in the challenge.
	 */
	for (l = realms; l != NULL; l = l->next) {
		if (strcasecmp(p, l->value) == 0)
			return p;
	}
 use_1st_realm:
	if ((p = strdup(realms->value)) == NULL)
		goto nomem;
	return p;
 nomem:
	saslc__error_set_errno(ERR(sess), ERROR_NOMEM);
	return NULL;
}

/**
 * @brief destroy a cipher context
 * @param ctx cipher context
 * @return nothing
 */
static void
cipher_context_destroy(cipher_context_t *ctx)
{

	if (ctx != NULL) {
		if (ctx->evp_ctx != NULL)
			EVP_CIPHER_CTX_free(ctx->evp_ctx);
		free(ctx);
	}
}

/**
 * @brief slide the bits from 7 bytes into the high 7 bits of 8 bites
 * @param ikey input key
 * @param okey output key
 *
 * This matches cyrus-sasl 2.1.23
 */
static inline void
slidebits(uint8_t *ikey, uint8_t *okey)
{

	okey[0] = ikey[0] << 0;
	okey[1] = ikey[0] << 7 | (unsigned)ikey[1] >> 1;
	okey[2] = ikey[1] << 6 | (unsigned)ikey[2] >> 2;
	okey[3] = ikey[2] << 5 | (unsigned)ikey[3] >> 3;
	okey[4] = ikey[3] << 4 | (unsigned)ikey[4] >> 4;
	okey[5] = ikey[4] << 3 | (unsigned)ikey[5] >> 5;
	okey[6] = ikey[5] << 2 | (unsigned)ikey[6] >> 6;
	okey[7] = ikey[6] << 1;
}

/**
 * @brief convert our key to a DES key
 * @param key our key
 * @param keylen our key length
 * @param deskey the key in DES format
 *
 * NOTE: The openssl implementations of "des" and "3des" expect their
 * keys to be in the high 7 bits of 8 bytes and 16 bytes,
 * respectively.  Thus, our key length will be 7 and 14 bytes,
 * respectively.
 */
static void
make_deskey(uint8_t *key, size_t keylen, uint8_t *deskey)
{

	assert(keylen == 7 || keylen == 14);

	slidebits(deskey + 0, key + 0);
	if (keylen == 14)
		slidebits(deskey + 7, key + 7);
}

/**
 * @brief create a cipher context, including EVP cipher initialization.
 * @param sess session context
 * @param cipher cipher to use
 * @param do_enc encode context if set, decode context if 0
 * @param key crypt key to use
 * @return cipher context, or NULL on error
 */
static cipher_context_t *
cipher_context_create(saslc_sess_t *sess, cipher_t cipher, int do_enc, uint8_t *key)
{
#define AES_IV_MAGIC		"aes-128"
#define AES_IV_MAGIC_LEN	(sizeof(AES_IV_MAGIC) - 1)
	static const struct cipher_ctx_tbl_s {
		cipher_t eval;			/* for error checking */
		const EVP_CIPHER *(*evp_type)(void);/* type of cipher */
		size_t keylen;			/* key length */
		ssize_t blksize;		/* block size for cipher */
		size_t ivlen;			/* initial value length */
	} cipher_ctx_tbl[] = {
		/* NB: table indexed by cipher_t */
		/* eval		 evp_type	 keylen  blksize  ivlen */
		{ CIPHER_DES,    EVP_des_cbc,       7,       8,      8 },
		{ CIPHER_3DES,   EVP_des_ede_cbc,  14,       8,      8 },
		{ CIPHER_RC4,    EVP_rc4,          16,       1,      0 },
		{ CIPHER_RC4_40, EVP_rc4,           5,       1,      0 },
		{ CIPHER_RC4_56, EVP_rc4,           7,       1,      0 },
		{ CIPHER_AES,    EVP_aes_128_cbc,  16,      16,     16 }
	};
	const struct cipher_ctx_tbl_s *ctp;
	char buf[sizeof(md5hash_t) + AES_IV_MAGIC_LEN];
	uint8_t deskey[16];
	md5hash_t aes_iv;		/* initial value buffer for aes */
	cipher_context_t *ctx;		/* cipher context */
	uint8_t *ivp;
	const char *errmsg;
	int rv;

	/*************************************************************************/
	/* See draft-ietf-sasl-rfc2831bis-02.txt section 2.4 (mentions "aes")    */
	/* The key for the "rc4" and "aes" ciphers is all 16 bytes of Kcc or Kcs.*/
	/* The key for the "rc4-40" cipher is the first 5 bytes of Kcc or Kcs.   */
	/* The key for the "rc4-56" is the first 7 bytes of Kcc or Kcs.          */
	/* The key for "des" is the first 7 bytes of Kcc or Kcs.                 */
	/* The key for "3des" is the first 14 bytes of Kcc or Kcs.               */
	/*                                                                       */
	/* The IV used to send/receive the initial buffer of security encoded    */
	/* data for "des" and "3des" is the last 8 bytes of Kcc or Kcs. For all  */
	/* subsequent buffers the last 8 bytes of the ciphertext of the buffer   */
	/* NNN is used as the IV for the buffer (NNN + 1).                       */
	/*                                                                       */
	/* The IV for the "aes" cipher in CBC mode for messages going from the   */
	/* client to the server (IVc) consists of 16 bytes calculated as         */
	/* follows: IVc = MD5({Kcc, "aes-128"})                                  */
	/*                                                                       */
	/* The IV for the "aes" cipher in CBC mode for messages going from the   */
	/* server to the client (IVs) consists of 16 bytes calculated as         */
	/* follows: IVs = MD5({Kcs, "aes-128"})                                  */
	/*************************************************************************/

	assert(cipher < __arraycount(cipher_ctx_tbl));
	if (cipher >= __arraycount(cipher_ctx_tbl)) {
		saslc__error_set_errno(ERR(sess), ERROR_BADARG);
		return NULL;
	}

	ctx = malloc(sizeof(*ctx));
	if (ctx == NULL) {
		saslc__error_set_errno(ERR(sess), ERROR_NOMEM);
		return NULL;
	}

	ctp = &cipher_ctx_tbl[cipher];
	assert(ctp->eval == cipher);

	ctx->blksize = ctp->blksize;

	ctx->evp_ctx = EVP_CIPHER_CTX_new();
	if (ctx->evp_ctx == NULL) {
		errmsg = "EVP_CIPHER_CTX_new failed";
		goto err;
	}
	if (EVP_CipherInit_ex(ctx->evp_ctx, ctp->evp_type(), NULL, NULL, NULL,
								do_enc) == 0) {
		errmsg = "EVP_CipherInit_ex failed";
		goto err;
	}
	if (EVP_CIPHER_CTX_set_padding(ctx->evp_ctx, 0) == 0) {
		errmsg = "EVP_CIPHER_CTX_set_padding failed";
		goto err;
	}
	ivp = NULL;
	switch (cipher) {	/* prepare key and IV */
	case CIPHER_RC4:
	case CIPHER_RC4_40:
	case CIPHER_RC4_56:
		assert(ctp->ivlen == 0);	/* no IV */
		rv = EVP_CIPHER_CTX_set_key_length(ctx->evp_ctx,
		    (int)ctp->keylen);
		if (rv == 0) {
			errmsg = "EVP_CIPHER_CTX_set_key_length failed";
			goto err;
		}
		break;
	case CIPHER_DES:
	case CIPHER_3DES:
		assert(ctp->ivlen == 8);
		ivp = key + 8;
		make_deskey(key, ctp->keylen, deskey);
		key = deskey;
		break;
	case CIPHER_AES:
		assert(ctp->ivlen == 16);
		/* IVs = MD5({Kcs, "aes-128"}) */
		memcpy(buf, key, sizeof(md5hash_t));
		memcpy(buf + sizeof(md5hash_t), AES_IV_MAGIC, AES_IV_MAGIC_LEN);
		saslc__crypto_md5_hash(buf, sizeof(buf), aes_iv);
		ivp = aes_iv;
		break;
	}
	if (EVP_CipherInit_ex(ctx->evp_ctx, NULL, NULL, key, ivp, do_enc) == 0) {
		errmsg = "EVP_CipherInit_ex 2 failed";
		goto err;
	}
	return ctx;
 err:
	cipher_context_destroy(ctx);
	saslc__error_set(ERR(sess), ERROR_MECH, errmsg);
	return NULL;

#undef AES_IV_MAGIC_LEN
#undef AES_IV_MAGIC
}

/**
 * @brief compute the necessary padding length
 * @param ctx the cipher context
 * @param inlen the data length to put in the packet
 * @return the length of padding needed (zero if none needed)
 */
static size_t
get_padlen(cipher_context_t *ctx, size_t inlen)
{
	size_t blksize;

	if (ctx == NULL)
		return 0;

	blksize = ctx->blksize;
	if (blksize == 1)
		return 0;

	return blksize - ((inlen + 10) % blksize);
}

/**
 * @brief compute the packet integrity including the version and
 * sequence number
 * @param key the hmac_md5 hash key
 * @param seqnum the sequence number
 * @param in the input buffer
 * @param inlen the input buffer length
 * @return 0 on success, -1 on failure
 */
static int
packet_integrity(md5hash_t key, uint32_t seqnum, void *in, size_t inlen,
    md5hash_t mac)
{

	be32enc(in, seqnum);
	if (saslc__crypto_hmac_md5_hash(key, MD5_DIGEST_LENGTH, in, inlen, mac)
	    == -1)
		return -1;

	/* we keep only the first 10 bytes of the hash */
	be16enc(mac + 10, 0x0001);	/* add 2 byte version number */
	be32enc(mac + 12, seqnum);	/* add 4 byte sequence number */
	return 0;
}

/**
 * @brief encode or decode a buffer (in place)
 * @param ctx the cipher context
 * @param in the input buffer
 * @param inlen the buffer length
 * @return the length of the result left in the input buffer after
 * processing, or -1 on failure.
 */
static ssize_t
cipher_update(cipher_context_t *ctx, void *in, size_t inlen)
{
	int outl, rv;
	void *out;

	out = in; /* XXX: this assumes we can encoded and decode in place */
	rv = EVP_CipherUpdate(ctx->evp_ctx, out, &outl, in, (int)inlen);
	if (rv == 0)
		return -1;

	return outl;
}

/**
 * @brief incapsulate a message with confidentiality (sign and encrypt)
 * @param ctx coder context
 * @param in pointer to message to encode
 * @param inlen length of message
 * @param out encoded output packet (including prefixed 4 byte length field)
 * @param outlen decoded output packet length
 * @returns 0 on success, -1 on failure
 *
 * NOTE: this allocates memory for its output and the caller is
 * responsible for freeing it.
 *
 * integrity (auth-int):
 * len, HMAC(ki, {SeqNum, msg})[0..9], x0001, SeqNum
 *
 * confidentiality (auth-conf):
 * len, CIPHER(Kc, {msg, pag, HMAC(ki, {SeqNum, msg})[0..9]}), x0001, SeqNum
 */
static ssize_t
encode_buffer(coder_context_t *ctx, const void *in, size_t inlen,
    void **out, size_t *outlen)
{
	void *buf;
	uint8_t *mac, *p;
	ssize_t tmplen;
	size_t buflen;
	size_t padlen;

	padlen = get_padlen(ctx->cph_ctx, inlen);
	buflen = 4 + inlen + padlen + sizeof(md5hash_t);
	buf = malloc(buflen);
	if (buf == NULL) {
		saslc__error_set_errno(ERR(ctx->sess), ERROR_NOMEM);
		return -1;
	}
	p = buf;
	memcpy(p + 4, in, inlen);
	mac = p + 4 + inlen + padlen;
	if (packet_integrity(ctx->key, ctx->seqnum, buf, 4 + inlen, mac)
	    == -1) {
		saslc__error_set(ERR(ctx->sess), ERROR_MECH, "HMAC failed");
		free(buf);
		return -1;
	}

	if (padlen)
		memset(p + 4 + inlen, (int)padlen, padlen);

	if (ctx->cph_ctx != NULL) {
		if ((tmplen = cipher_update(ctx->cph_ctx, p + 4,
			 inlen + padlen + 10)) == -1) {
			saslc__error_set(ERR(ctx->sess), ERROR_MECH,
			    "cipher error");
			free(buf);
			return -1;
		}
		assert((size_t)tmplen == inlen + padlen + 10);
		if ((size_t)tmplen != inlen + padlen + 10)
			return -1;
	}

	be32enc(buf, (uint32_t)(buflen - 4));

	*out = buf;
	*outlen = buflen;
	ctx->seqnum++;		/* wraps at 2^32 */
	return 0;
}

/**
 * @brief decode one complete confidentiality encoded packet
 * @param ctx coder context
 * @param in pointer to packet, including the beginning 4 byte length field.
 * @param inlen length of packet
 * @param out decoded output
 * @param outlen decoded output length
 * @returns 0 on success, -1 on failure
 *
 * NOTE: this modifies the intput buffer!
 * NOTE: this allocates memory for its output and the caller is
 * responsible for freeing it.
 *
 * integrity (auth-int):
 * len, HMAC(ki, {SeqNum, msg})[0..9], x0001, SeqNum
 *
 * confidentiality (auth-conf):
 * len, CIPHER(Kc, {msg, pag, HMAC(ki, {SeqNum, msg})[0..9]}), x0001, SeqNum
 */
static ssize_t
decode_buffer(coder_context_t *ctx, void *in, size_t inlen,
    void **out, size_t *outlen)
{
	md5hash_t mac;
	void *buf;
	uint8_t *p;
	size_t blksize, buflen, padlen;
	ssize_t tmplen;
	uint32_t len;

	padlen = get_padlen(ctx->cph_ctx, 1);
	if (inlen < 4 + 1 + padlen + MD5_DIGEST_LENGTH) {
		saslc__error_set(ERR(ctx->sess), ERROR_MECH,
		    "zero payload packet");
		return -1;
	}
	len = be32dec(in);
	if (len + 4 != inlen) {
		saslc__error_set(ERR(ctx->sess), ERROR_MECH,
		    "bad packet length");
		return -1;
	}

	if (ctx->cph_ctx != NULL) {
		p = in;
		if ((tmplen = cipher_update(ctx->cph_ctx, p + 4, len - 6)) == -1) {
			saslc__error_set(ERR(ctx->sess), ERROR_MECH,
			    "cipher error");
			return -1;
		}
		assert(tmplen == (ssize_t)len - 6);
		if (tmplen != (ssize_t)len - 6)
			return -1;
	}

	blksize = ctx->cph_ctx ? ctx->cph_ctx->blksize : 0;
	if (blksize <= 1)
		padlen = 0;
	else{
		p = in;
		padlen = p[inlen - sizeof(md5hash_t) - 1];
		if (padlen > blksize || padlen == 0) {
			saslc__error_set(ERR(ctx->sess), ERROR_MECH,
			    "invalid padding length after decode");
			return -1;
		}
	}
	if (packet_integrity(ctx->key, ctx->seqnum, in,
				inlen - padlen - sizeof(mac), mac) == -1) {
		saslc__error_set(ERR(ctx->sess), ERROR_MECH, "HMAC failed");
		return -1;
	}

	p = in;
	p += 4 + len - MD5_DIGEST_LENGTH;
	if (memcmp(p, mac, MD5_DIGEST_LENGTH) != 0) {
		uint32_t seqnum;

		p = in;
		seqnum = be32dec(p + inlen - 4);
		saslc__error_set(ERR(ctx->sess), ERROR_MECH,
		    seqnum != ctx->seqnum ? "invalid MAC (bad seqnum)" :
		    "invalid MAC");
		return -1;
	}

	buflen = len - padlen - MD5_DIGEST_LENGTH;
	buf = malloc(buflen);
	if (buf == NULL) {
		saslc__error_set_errno(ERR(ctx->sess), ERROR_NOMEM);
		return -1;
	}
	p = in;
	p += 4;
	memcpy(buf, p, buflen);

	*out = buf;
	*outlen = buflen;
	ctx->seqnum++;
	return 0;
}

/**
 * @brief add integrity or confidentiality layer
 * @param sess session handle
 * @param in input buffer
 * @param inlen input buffer length
 * @param out pointer to output buffer
 * @param out pointer to output buffer length
 * @return number of bytes consumed on success, 0 if insufficient data
 * to process, -1 on failure
 */
static ssize_t
saslc__mech_digestmd5_encode(saslc_sess_t *sess, const void *in, size_t inlen,
    void **out, size_t *outlen)
{
	saslc__mech_digestmd5_sess_t *ms;
	uint8_t *buf;
	size_t buflen;
	ssize_t rval;

	ms = sess->mech_sess;
	assert(ms->mech_sess.qop != QOP_NONE);
	if (ms->mech_sess.qop == QOP_NONE)
		return -1;

	rval = saslc__buffer_fetch(ms->enc_ctx.buf_ctx, in, inlen, &buf, &buflen);
	if (rval == -1)
		return -1;
	if (buflen == 0) {
		*out = NULL;
		*outlen = 0;
		return rval;
	}
	if (encode_buffer(&ms->enc_ctx, buf, buflen, out, outlen) == -1)
		return -1;

	return rval;
}

/**
 * @brief remove integrity or confidentiality layer
 * @param sess session handle
 * @param in input buffer
 * @param inlen input buffer length
 * @param out pointer to output buffer
 * @param out pointer to output buffer length
 * @return number of bytes consumed on success, 0 if insufficient data
 * to process, -1 on failure
 *
 * integrity (auth-int):
 * len, HMAC(ki, {SeqNum, msg})[0..9], x0001, SeqNum
 *
 * confidentiality (auth-conf):
 * len, CIPHER(Kc, {msg, pag, HMAC(ki, {SeqNum, msg})[0..9]}), x0001, SeqNum
 */
static ssize_t
saslc__mech_digestmd5_decode(saslc_sess_t *sess, const void *in, size_t inlen,
    void **out, size_t *outlen)
{
	saslc__mech_digestmd5_sess_t *ms;
	uint8_t *buf;
	size_t buflen;
	ssize_t rval;

	ms = sess->mech_sess;
	assert(ms->mech_sess.qop != QOP_NONE);
	if (ms->mech_sess.qop == QOP_NONE)
		return -1;

	rval = saslc__buffer32_fetch(ms->dec_ctx.buf_ctx, in, inlen, &buf, &buflen);
	if (rval == -1)
		return -1;

	if (buflen == 0) {
		*out = NULL;
		*outlen = 0;
		return rval;
	}
	if (decode_buffer(&ms->dec_ctx, buf, buflen, out, outlen) == -1)
		return -1;

	return rval;
}

/************************************************************************
 * XXX: Share with mech_gssapi.c?  They are almost identical.
 */
/**
 * @brief choose the best qop based on what was provided by the
 * challenge and a possible user mask.
 * @param sess the session context
 * @param qop_flags the qop flags parsed from the challenge string
 * @return the selected saslc__mech_sess_qop_t or -1 if no match
 */
static int
choose_qop(saslc_sess_t *sess, uint32_t qop_flags)
{
	list_t *list;
	const char *user_qop;

	if (qop_flags == 0)	/* no qop spec in challenge (it's optional) */
		return QOP_NONE;

	qop_flags &= DEFAULT_QOP_MASK;
	user_qop = saslc_sess_getprop(sess, SASLC_DIGESTMD5_QOPMASK);
	if (user_qop != NULL) {
		if (saslc__list_parse(&list, user_qop) == -1) {
			saslc__error_set_errno(ERR(sess), ERROR_NOMEM);
			return -1;
		}
		qop_flags &= saslc__mech_qop_list_flags(list);
		saslc__list_free(list);
	}

	/*
	 * Select the most secure supported qop.
	 */
	if ((qop_flags & F_QOP_CONF) != 0)
		return QOP_CONF;
	if ((qop_flags & F_QOP_INT) != 0)
		return QOP_INT;
	if ((qop_flags & F_QOP_NONE) != 0)
		return QOP_NONE;

	saslc__error_set(ERR(sess), ERROR_MECH,
	    "cannot choose an acceptable qop");
	return -1;
}
/************************************************************************/

/**
 * @brief choose the best cipher based on what was provided by the
 * challenge and a possible user mask.
 * @param sess the session context
 * @param cipher_flags the cipher flags parsed from the challenge
 * string
 * @return the selected cipher_t
 */
static int
choose_cipher(saslc_sess_t *sess, unsigned int cipher_flags)
{
	list_t *list;
	unsigned int cipher_mask;
	const char *user_cipher;

	if (cipher_flags == 0) {
		saslc__error_set(ERR(sess), ERROR_MECH,
		    "no cipher spec in challenge");
		return -1;
	}
	cipher_mask = DEFAULT_CIPHER_MASK;
	user_cipher = saslc_sess_getprop(sess, SASLC_DIGESTMD5_CIPHERMASK);
	if (user_cipher != NULL) {
		if (saslc__list_parse(&list, user_cipher) == -1) {
			saslc__error_set_errno(ERR(sess), ERROR_NOMEM);
			return -1;
		}
		cipher_mask = cipher_list_flags(list);
		saslc__list_free(list);
	}
	cipher_flags &= cipher_mask;

	/*
	 * Select the most secure cipher supported.
	 * XXX: Is the order here right?
	 */
	if ((cipher_flags & F_CIPHER_AES) != 0)
		return CIPHER_AES;
	if ((cipher_flags & F_CIPHER_3DES) != 0)
		return CIPHER_3DES;
	if ((cipher_flags & F_CIPHER_DES) != 0)
		return CIPHER_DES;
	if ((cipher_flags & F_CIPHER_RC4) != 0)
		return CIPHER_RC4;
	if ((cipher_flags & F_CIPHER_RC4_56) != 0)
		return CIPHER_RC4_56;
	if ((cipher_flags & F_CIPHER_RC4_40) != 0)
		return CIPHER_RC4_40;

	saslc__error_set(ERR(sess), ERROR_MECH,
	    "qop \"auth-conf\" requires a cipher");
	return -1;
}

/**
 * @brief get the challenge_t value corresponding to a challenge key
 * string.
 * @param key challenge key string
 * @return the challenge_t value including CHALLENGE_IGNORE (-1) if
 * the key is not recognized
 */
static challenge_t
get_challenge_t(const char *key)
{
	static const struct {
		const char *key;
		challenge_t value;
	} challenge_keys[] = {
		{ "realm",	CHALLENGE_REALM     },
		{ "nonce",	CHALLENGE_NONCE     },
		{ "qop",	CHALLENGE_QOP       },
		{ "stale",	CHALLENGE_STALE     },
		{ "maxbuf",	CHALLENGE_MAXBUF    },
		{ "charset",	CHALLENGE_CHARSET   },
		{ "algorithm",	CHALLENGE_ALGORITHM },
		{ "cipher",	CHALLENGE_CIPHER    }
	};
	size_t i;

	for (i = 0; i < __arraycount(challenge_keys); i++) {
		if (strcasecmp(key, challenge_keys[i].key) == 0)
			return challenge_keys[i].value;
	}
	return CHALLENGE_IGNORE;
}

/**
 * @brief parses challenge and store result in mech_sess.
 * @param mech_sess session where parsed data will be stored
 * @param challenge challenge
 * @return 0 on success, -1 on failure.
 */
static int
saslc__mech_digestmd5_parse_challenge(saslc_sess_t *sess, const char *challenge)
{
	saslc__mech_digestmd5_sess_t *ms;
	list_t *list, *n;
	list_t *tmp_list;
	cdata_t *cdata;
	size_t maxbuf;
	uint32_t tmp_flags;
	int rv;

	/******************************************************************/
	/* digest-challenge  =                                            */
	/*     1#( realm | nonce | qop-options | stale | server_maxbuf |  */
	/*      charset | algorithm | cipher-opts | auth-param )          */
	/******************************************************************/

	saslc__msg_dbg("challenge: '%s'\n", challenge);

	ms = sess->mech_sess;
	cdata = &ms->cdata;

	rv = -1;
	memset(cdata, 0, sizeof(*cdata));
	if (saslc__list_parse(&list, challenge) == -1) {
		saslc__error_set_errno(ERR(sess), ERROR_NOMEM);
		return -1;
	}
	saslc__list_log(list, "parse list:\n");
	for (n = list; n != NULL; n = n->next) {
		char *key;
		char *val;

		/* Split string into key and val */
		key = n->value;
		val = strchr(key, '=');
		if (val == NULL)
			goto no_mem;
		*val = '\0';
		val = strip_quotes(val + 1);

		saslc__msg_dbg("key='%s' val='%s'\n", key, val);
		switch (get_challenge_t(key)) {
		case CHALLENGE_REALM:
			/**************************************************/
			/* realm       = "realm" "=" <"> realm-value <">  */
			/* realm-value = qdstr-val                        */
			/*                                                */
			/* This directive is optional; if not present,    */
			/* the client SHOULD solicit it from the user or  */
			/* be able to compute a default; a plausible      */
			/* default might be the realm supplied by the     */
			/* user when they logged in to the client system. */
			/* Multiple realm directives are allowed, in      */
			/* which case the user or client must choose one  */
			/* as the realm for which to supply to username   */
			/* and password.                                  */
			/**************************************************/
			if (saslc__list_append(&cdata->realm, val) == -1)
				goto no_mem;
			break;
		case CHALLENGE_NONCE:
			/**************************************************/
			/* nonce       = "nonce" "=" <"> nonce-value <">  */
			/* nonce-value = *qdtext                          */
			/*                                                */
			/* This directive is required and MUST appear     */
			/* exactly once; if not present, or if multiple   */
			/* instances are present, the client should abort */
			/* the authentication exchange.                   */
			/**************************************************/
			if (cdata->nonce != NULL) {
				saslc__error_set(ERR(sess), ERROR_MECH,
				    "multiple nonce in challenge");
				goto out;
			}
			cdata->nonce = strdup(val);
			if (cdata->nonce == NULL)
				goto no_mem;
			break;
		case CHALLENGE_QOP:
			/**************************************************/
			/* qop-options = "qop" "=" <"> qop-list <">       */
			/* qop-list    = 1#qop-value                      */
			/* qop-value   = "auth" | "auth-int" |            */
			/*               "auth-conf" | token              */
			/*                                                */
			/* This directive is optional; if not present it  */
			/* defaults to "auth".  The client MUST ignore    */
			/* unrecognized options; if the client recognizes */
			/* no option, it should abort the authentication  */
			/* exchange.                                      */
			/**************************************************/
			if (saslc__list_parse(&tmp_list, val) == -1)
				goto no_mem;
			saslc__list_log(tmp_list, "qop list:\n");
			tmp_flags = saslc__mech_qop_list_flags(tmp_list);
			saslc__list_free(tmp_list);
			if (tmp_flags == 0) {
				saslc__error_set(ERR(sess), ERROR_MECH,
				    "qop required in challenge");
				goto out;
			}
			cdata->qop_flags |= tmp_flags;
			break;
		case CHALLENGE_STALE:
			/**************************************************/
			/* stale = "stale" "=" "true"                     */
			/*                                                */
			/* This directive may appear at most once; if     */
			/* multiple instances are present, the client     */
			/* should abort the authentication exchange.      */
			/**************************************************/
			if (cdata->stale) {
				saslc__error_set(ERR(sess), ERROR_MECH,
				    "multiple stale in challenge");
				goto out;
			}
			if (strcasecmp(val, "true") != 0) {
				saslc__error_set(ERR(sess), ERROR_MECH,
				    "stale must be true");
				goto out;
			}
			cdata->stale = true;
			break;
		case CHALLENGE_MAXBUF:
			/**************************************************/
			/* maxbuf-value = 1*DIGIT                         */
			/*                                                */
			/* The value MUST be bigger than 16 and smaller   */
			/* or equal to 16777215 (i.e.  2**24-1). If this  */
			/* directive is missing, the default value is     */
			/* 65536. This directive may appear at most once; */
			/* if multiple instances are present, the client  */
			/* MUST abort the authentication exchange.        */
			/**************************************************/
			if (cdata->maxbuf != 0) {
				saslc__error_set(ERR(sess), ERROR_MECH,
				    "multiple maxbuf in challenge");
				goto out;
			}
			maxbuf = (size_t)strtoul(val, NULL, 10);
			if (INVALID_MAXBUF(maxbuf)) {
				saslc__error_set(ERR(sess), ERROR_MECH,
				    "invalid maxbuf in challenge");
				goto out;
			}
			cdata->maxbuf = maxbuf;
			break;
		case CHALLENGE_CHARSET:
			/**************************************************/
			/* charset = "charset" "=" "utf-8"                */
			/*                                                */
			/* This directive may appear at most once; if     */
			/* multiple instances are present, the client     */
			/* should abort the authentication exchange.      */
			/**************************************************/
			if (cdata->utf8) {
				saslc__error_set(ERR(sess), ERROR_MECH,
				    "multiple charset in challenge");
				goto out;
			}
			if (strcasecmp(val, "utf-8") != 0) {
				saslc__error_set(ERR(sess), ERROR_MECH,
				    "charset != \"utf-8\" in challenge");
				goto out;
			}
			cdata->utf8 = true;
			break;
		case CHALLENGE_ALGORITHM:
			/**************************************************/
			/* algorithm = "algorithm" "=" "md5-sess"         */
			/*                                                */
			/* This directive is required and MUST appear     */
			/* exactly once; if not present, or if multiple   */
			/* instances are present, the client should abort */
			/* the authentication exchange.                   */
			/**************************************************/
			if (cdata->algorithm) {
				saslc__error_set(ERR(sess), ERROR_MECH,
				    "multiple algorithm in challenge");
				goto out;
			}
			if (strcasecmp(val, "md5-sess") != 0) {
				saslc__error_set(ERR(sess), ERROR_MECH,
				    "algorithm != \"md5-sess\" in challenge");
				goto out;
			}
			cdata->algorithm = true;
			break;
		case CHALLENGE_CIPHER:
			/**************************************************/
			/* cipher-opts = "cipher" "=" <"> 1#cipher-val <">*/
			/* cipher-val  = "3des" | "des" | "rc4-40" |      */
			/*               "rc4" |"rc4-56" | "aes" |        */
			/*               token                            */
			/*                                                */
			/* This directive must be present exactly once if */
			/* "auth-conf" is offered in the "qop-options"    */
			/* directive, in which case the "3des" cipher is  */
			/* mandatory-to-implement. The client MUST ignore */
			/* unrecognized options; if the client recognizes */
			/* no option, it should abort the authentication  */
			/* exchange.                                      */
			/**************************************************/
			if (saslc__list_parse(&tmp_list, val) == -1)
				goto no_mem;
			saslc__list_log(tmp_list, "cipher list:\n");
			tmp_flags = cipher_list_flags(tmp_list);
			saslc__list_free(tmp_list);
			if (tmp_flags == 0) {
				saslc__error_set(ERR(sess), ERROR_MECH,
				    "unknown cipher");
				goto out;
			}
			cdata->cipher_flags |= tmp_flags;
			break;
		case CHALLENGE_IGNORE:
			/**************************************************/
			/* auth-param = token "=" ( token |               */
			/*                          quoted-string )       */
			/*                                                */
			/* The client MUST ignore any unrecognized        */
			/* directives.                                    */
			/**************************************************/
			break;
		}
	}

	/*
	 * make sure realms are in iso8859-1
	 */
	if (stringprep_realms(cdata->utf8, cdata->realm) == -1) {
		saslc__error_set(ERR(sess), ERROR_MECH,
		    "unable to convert realms in challenge from "
		    "\"utf-8\" to iso8859-1");
		goto out;
	}

	/*
	 * test for required options
	 */
	if (cdata->nonce == NULL) {
		saslc__error_set(ERR(sess), ERROR_MECH,
		    "nonce required in challenge");
		goto out;
	}

	if (!cdata->algorithm) {
		saslc__error_set(ERR(sess), ERROR_MECH,
		    "algorithm required in challenge");
		goto out;
	}

	/*
	 * set the default maxbuf value if it was missing from the
	 * challenge.
	 */
	if (cdata->maxbuf == 0)
		cdata->maxbuf = DEFAULT_MAXBUF;

	saslc__msg_dbg("qop_flags=0x%04x\n",    cdata->qop_flags);
	saslc__msg_dbg("cipher_flags=0x%04x\n", cdata->cipher_flags);

	rv = 0;
 out:
	saslc__list_free(list);
	return rv;
 no_mem:
	saslc__error_set_errno(ERR(sess), ERROR_NOMEM);
	goto out;
}

/**
 * @brief creates digestmd5 mechanism session.
 * Function initializes also default options for the session.
 * @param sess sasl session
 * @return 0 on success, -1 on failure.
 */
static int
saslc__mech_digestmd5_create(saslc_sess_t *sess)
{
	saslc__mech_digestmd5_sess_t *c;

	if ((c = calloc(1, sizeof(*c))) == NULL) {
		saslc__error_set_errno(ERR(sess), ERROR_NOMEM);
		return -1;
	}
	c->rdata.nonce_cnt = 1;
	sess->mech_sess = c;

	return 0;
}

static void
free_cdata(cdata_t *cdata)
{

	free(cdata->nonce);
	saslc__list_free(cdata->realm);
}

static void
free_rdata(rdata_t *rdata)
{

	free(rdata->authcid);
	free(rdata->authzid);
	free(rdata->cnonce);
	free(rdata->digesturi);
	if (rdata->passwd != NULL) {
		memset(rdata->passwd, 0, strlen(rdata->passwd));
		free(rdata->passwd);
	}
	free(rdata->realm);
}

/**
 * @brief destroys digestmd5 mechanism session.
 * Function also is freeing assigned resources to the session.
 * @param sess sasl session
 * @return Functions always returns 0.
 */
static int
saslc__mech_digestmd5_destroy(saslc_sess_t *sess)
{
	saslc__mech_digestmd5_sess_t *ms;

	ms = sess->mech_sess;

	free_cdata(&ms->cdata);
	free_rdata(&ms->rdata);

	saslc__buffer32_destroy(ms->dec_ctx.buf_ctx);
	saslc__buffer_destroy(ms->enc_ctx.buf_ctx);

	cipher_context_destroy(ms->dec_ctx.cph_ctx);
	cipher_context_destroy(ms->enc_ctx.cph_ctx);

	free(sess->mech_sess);
	sess->mech_sess = NULL;

	return 0;
}

/**
 * @brief collect the response data necessary to build the reply.
 * @param sess the session context
 * @return 0 on success, -1 on failure
 *
 * NOTE:
 * The input info is from the challenge (previously saved in cdata of
 * saslc__mech_digestmd5_sess_t) or from the property dictionaries.
 *
 * The output info is saved in (mostly) in rdata of the
 * saslc__mech_digestmd5_sess_t structure.  The qop is special in that
 * it is exposed to the saslc__mech_sess_t layer.
 */
static int
saslc__mech_digestmd5_response_data(saslc_sess_t *sess)
{
	saslc__mech_digestmd5_sess_t *ms;
	cdata_t *cdata;
	rdata_t *rdata;
	const char *authcid;
	const char *authzid;
	const char *hostname;
	const char *maxbuf;
	const char *passwd;
	int rv;

	ms = sess->mech_sess;
	cdata = &ms->cdata;
	rdata = &ms->rdata;

	if ((rv = choose_qop(sess, cdata->qop_flags)) == -1)
		return -1;	/* error message already set */
	ms->mech_sess.qop = rv;

	if (ms->mech_sess.qop == QOP_CONF) {
		if ((rv = choose_cipher(sess, cdata->cipher_flags)) == -1)
			return -1;	/* error message already set */
		rdata->cipher = rv;
	}

	hostname = saslc_sess_getprop(sess, SASLC_DIGESTMD5_HOSTNAME);
	if (hostname == NULL) {
		saslc__error_set(ERR(sess), ERROR_MECH,
		    "hostname is required for authentication");
		return -1;
	}

	rdata->realm = choose_realm(sess, hostname, cdata->realm);
	if (rdata->realm == NULL)
		return -1;	/* error message already set */

	rdata->digesturi = saslc__mech_digestmd5_digesturi(sess, hostname);
	if (rdata->digesturi == NULL)
		return -1;	/* error message already set */

	authcid = saslc_sess_getprop(sess, SASLC_DIGESTMD5_AUTHCID);
	if (authcid == NULL) {
		saslc__error_set(ERR(sess), ERROR_MECH,
		    "authcid is required for an authentication");
		return -1;
	}
	rdata->authcid = strdup(authcid);
	if (rdata->authcid == NULL)
		goto no_mem;

	authzid = saslc_sess_getprop(sess, SASLC_DIGESTMD5_AUTHZID);
	if (authzid != NULL) {
		rdata->authzid = strdup(authzid);
		if (rdata->authzid == NULL)
			goto no_mem;
	}

	passwd = saslc_sess_getprop(sess, SASLC_DIGESTMD5_PASSWD);
	if (passwd == NULL) {
		saslc__error_set(ERR(sess), ERROR_MECH,
		    "password is required for an authentication");
		return -1;
	}
	rdata->passwd = strdup(passwd);
	if (rdata->passwd == NULL)
		goto no_mem;

	rdata->cnonce = saslc__mech_digestmd5_nonce(NONCE_LEN);
	if (rdata->cnonce == NULL) {
		saslc__error_set(ERR(sess), ERROR_MECH,
		    "failed to create cnonce");
		return -1;
	}
#ifdef SASLC_DIGESTMD5_CNONCE	/* XXX: for debugging! */
	{
		const char *cnonce;
		cnonce = saslc_sess_getprop(sess, SASLC_DIGESTMD5_CNONCE);
		if (cnonce != NULL) {
			rdata->cnonce = strdup(cnonce);
			if (rdata->cnonce == NULL)
				goto no_mem;
		}
	}
#endif
	if (ms->mech_sess.qop != QOP_NONE) {
		maxbuf = saslc_sess_getprop(sess, SASLC_DIGESTMD5_MAXBUF);
		if (maxbuf != NULL)
			rdata->maxbuf = (size_t)strtoul(maxbuf, NULL, 10);
		if (rdata->maxbuf == 0)
			rdata->maxbuf = cdata->maxbuf;
		if (INVALID_MAXBUF(rdata->maxbuf)) {
			saslc__error_set(ERR(sess), ERROR_MECH,
			    "maxbuf out of range");
			return -1;
		}
	}
	return 0;

 no_mem:
	saslc__error_set_errno(ERR(sess), ERROR_NOMEM);
	return -1;
}

/**
 * @brief compute the maximum payload that can go into an integrity or
 * confidentiality packet.
 * @param maxbuf the server's maxbuf size.
 * @param blksize the ciphers block size.  0 or 1 if there is no blocking.
 * @return the payload size
 *
 * The packet (not including the leading uint32_t packet length field)
 * has this structure:
 *
 * struct {
 *    uint8_t payload[];	// packet payload
 *    uint8_t padding[];	// padding to block size
 *    uint8_t hmac_0_9[10];	// the first 10 bytes of the hmac
 *    uint8_t version[2];	// version number (1) in BE format
 *    uint8_t seqnum[4];	// sequence number in BE format
 * } __packed
 *
 * NOTE: if the block size is > 1, then padding is required to make
 * the {payload[], padding[], and hmac_0_9[]} a multiple of the block
 * size.  Furthermore there must be at least one byte of padding!  The
 * padding bytes are all set to the padding length and one byte of
 * padding is necessary to recover the padding length.
 */
static size_t
maxpayload(size_t maxbuf, size_t blksize)
{
	size_t l;

	if (blksize <= 1) {	/* no padding used */
		if (maxbuf <= sizeof(md5hash_t))
			return 0;

		return maxbuf - sizeof(md5hash_t);
	}
	if (maxbuf < 2 * blksize + 6)
		return 0;

	l = rounddown(maxbuf - 6, blksize);
	if (l <= 10 + 1)	/* we need at least one byte of padding */
		return 0;

	return l - 10 - 1;
}

/**
 * @brief initialize the encode and decode coder contexts for the session
 * @param sess the current session
 * @return 0 on success, -1 on failure.
 */
static int
init_coder_context(saslc_sess_t *sess)
{
	saslc__mech_digestmd5_sess_t *ms;
	size_t blksize;
#ifdef SASLC_DIGESTMD5_SELFTEST
	int selftest;		/* XXX: allow for testing against ourselves */
#endif

	ms = sess->mech_sess;
#ifdef SASLC_DIGESTMD5_SELFTEST
	selftest = saslc_sess_getprop(sess, SASLC_DIGESTMD5_SELFTEST) != NULL;
#endif
	blksize = 0;
	switch (ms->mech_sess.qop) {
	case QOP_NONE:
		return 0;
	case QOP_INT:
#ifdef	SASLC_DIGESTMD5_SELFTEST
		ms->dec_ctx.key = selftest ? ms->keys.kic : ms->keys.kis;
#else
		ms->dec_ctx.key = ms->keys.kis;
#endif
		ms->enc_ctx.key = ms->keys.kic;
		ms->dec_ctx.cph_ctx = NULL;
		ms->enc_ctx.cph_ctx = NULL;
		break;
	case QOP_CONF:
#ifdef	SASLC_DIGESTMD5_SELFTEST
		ms->dec_ctx.key = selftest ? ms->keys.kcc : ms->keys.kcs;
#else
		ms->dec_ctx.key = ms->keys.kcs;
#endif
		ms->enc_ctx.key = ms->keys.kcc;
		ms->dec_ctx.cph_ctx = cipher_context_create(sess,
		    ms->rdata.cipher, 0, ms->dec_ctx.key);
		if (ms->dec_ctx.cph_ctx == NULL)
			return -1;

		ms->enc_ctx.cph_ctx = cipher_context_create(sess,
		    ms->rdata.cipher, 1, ms->enc_ctx.key);
		if (ms->enc_ctx.cph_ctx == NULL)
			return -1;

		blksize = ms->enc_ctx.cph_ctx->blksize;
		break;
	}
	ms->dec_ctx.sess = sess;
	ms->enc_ctx.sess = sess;
	ms->dec_ctx.buf_ctx = saslc__buffer32_create(sess, ms->rdata.maxbuf);
	if (ms->cdata.maxbuf < 2 * blksize + 6) {
		saslc__error_set(ERR(sess), ERROR_MECH,
		    "server buffer too small for packet");
		return -1;
	}
	ms->enc_ctx.buf_ctx = saslc__buffer_create(sess,
	    maxpayload(ms->cdata.maxbuf, blksize));

	if (ms->dec_ctx.buf_ctx == NULL || ms->enc_ctx.buf_ctx == NULL) {
		saslc__error_set_errno(ERR(sess), ERROR_NOMEM);
		return -1;
	}
	return 0;
}

/**
 * @brief construct the reply string.
 * @param sess session context
 * @param response string
 * @return reply string or NULL on failure.
 */
static char *
saslc__mech_digestmd5_reply(saslc_sess_t *sess, char *response)
{
	saslc__mech_digestmd5_sess_t *ms;
	char *out;
	char *cipher, *maxbuf, *realm;

	ms = sess->mech_sess;

	out = NULL;
	cipher = __UNCONST("");
	maxbuf = __UNCONST("");
	realm = __UNCONST("");

	switch (ms->mech_sess.qop) {
	case QOP_CONF:
		if (asprintf(&cipher, "cipher=\"%s\",",
				cipher_name(ms->rdata.cipher)) == -1) {
			saslc__error_set_errno(ERR(sess), ERROR_NOMEM);
			goto done;
		}
		/*FALLTHROUGH*/
	case QOP_INT:
		if (asprintf(&maxbuf, "maxbuf=%zu,", ms->rdata.maxbuf) == -1) {
			saslc__error_set_errno(ERR(sess), ERROR_NOMEM);
			goto done;
		}
		break;
	case QOP_NONE:
		break;
	default:
		assert(/*CONSTCOND*/0);
		return NULL;
	}
	if (ms->rdata.realm != NULL &&
	    asprintf(&realm, "realm=\"%s\",", ms->rdata.realm) == -1) {
		saslc__error_set_errno(ERR(sess), ERROR_NOMEM);
		goto done;
	}

	if (asprintf(&out,
		"username=\"%s\","
		"%s"			/* realm= */
		"nonce=\"%s\","
		"cnonce=\"%s\","
		"nc=%08d,"
		"qop=%s,"
		"%s"			/* cipher= */
		"%s"			/* maxbuf= */
		"digest-uri=\"%s\","
		"response=%s",
		ms->rdata.authcid,
		realm,
		ms->cdata.nonce,
		ms->rdata.cnonce,
		ms->rdata.nonce_cnt,
		saslc__mech_qop_name(ms->mech_sess.qop),
		cipher,
		maxbuf,
		ms->rdata.digesturi,
		response
		    ) == -1) {
		saslc__error_set_errno(ERR(sess), ERROR_NOMEM);
		out = NULL;
	}
 done:
	if (realm[0] != '\0')
		free(realm);
	if (maxbuf[0] != '\0')
		free(maxbuf);
	if (cipher[0] != '\0')
		free(cipher);

	return out;
}

/**
 * @brief do one step of the sasl authentication
 * @param sess sasl session
 * @param in input data
 * @param inlen input data length
 * @param out place to store output data
 * @param outlen output data length
 * @return MECH_OK - authentication successful,
 * MECH_STEP - more steps are needed,
 * MECH_ERROR - error
 */
static int
saslc__mech_digestmd5_cont(saslc_sess_t *sess, const void *in, size_t inlen,
    void **out, size_t *outlen)
{
	saslc__mech_digestmd5_sess_t *ms;
	char *response;
	const char *p;

	ms = sess->mech_sess;

	switch(ms->mech_sess.step) {
	case 0:
		/* in case we are called before getting data from server */
		if (inlen == 0) {
			*out = NULL;
			*outlen = 0;
			return MECH_STEP;
		}
		/* if input data was provided, then doing the first step */
		ms->mech_sess.step++;
		/*FALLTHROUGH*/
	case 1:
		if (saslc__mech_digestmd5_parse_challenge(sess, in) == -1)
			return MECH_ERROR;

		if (saslc__mech_digestmd5_response_data(sess) == -1)
			return MECH_ERROR;

		if ((response = saslc__mech_digestmd5_response(ms,
			 "AUTHENTICATE")) == NULL) {
			saslc__error_set(ERR(sess), ERROR_MECH,
			    "unable to construct response");
			return MECH_ERROR;
		}
		*out = saslc__mech_digestmd5_reply(sess, response);
		free(response);
		if (out == NULL)
			return MECH_ERROR;

		*outlen = strlen(*out);
		return MECH_STEP;
	case 2:
		if ((response = saslc__mech_digestmd5_response(ms, ""))
		    == NULL) {
			saslc__error_set(ERR(sess), ERROR_MECH,
			    "unable to construct rspauth");
			return MECH_ERROR;
		}
		p = in;
		if (strncmp(p, "rspauth=", 8) != 0 ||
		    strcmp(response, p + 8) != 0) {
			saslc__msg_dbg("rspauth='%s'\n", response);
			saslc__error_set(ERR(sess), ERROR_MECH,
			    "failed to validate rspauth response");
			free(response);
			return MECH_ERROR;
		}
		free(response);
		if (init_coder_context(sess) == -1)
			return MECH_ERROR;
		*out = NULL;
		*outlen = 0;
		return MECH_OK;
	default:
		assert(/*CONSTCOND*/0); /* impossible */
		return MECH_ERROR;
	}
}

/* mechanism definition */
const saslc__mech_t saslc__mech_digestmd5 = {
	.name	 = "DIGEST-MD5",
	.flags	 = FLAG_MUTUAL | FLAG_DICTIONARY,
	.create	 = saslc__mech_digestmd5_create,
	.cont	 = saslc__mech_digestmd5_cont,
	.encode	 = saslc__mech_digestmd5_encode,
	.decode	 = saslc__mech_digestmd5_decode,
	.destroy = saslc__mech_digestmd5_destroy
};
