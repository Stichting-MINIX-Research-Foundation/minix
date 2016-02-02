/* $NetBSD: xsess.c,v 1.8 2015/08/08 10:38:35 shm Exp $ */

/*
 * Copyright (c) 2010 The NetBSD Foundation, Inc.
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
 * PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL THE FOUNDATION OR CONTRIBUTORS
 * BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 * CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
 * POSSIBILITY OF SUCH DAMAGE.
 */
#include <sys/cdefs.h>
__RCSID("$NetBSD: xsess.c,v 1.8 2015/08/08 10:38:35 shm Exp $");

#include <assert.h>
#include <saslc.h>
#include <stdio.h>
#include <string.h>

#include "crypto.h"
#include "dict.h"
#include "error.h"
#include "list.h"
#include "msg.h"
#include "mech.h"
#include "parser.h"
#include "saslc_private.h"

/*
 * TODO:
 *
 * 1) Add hooks to allow saslc_sess_encode() and saslc_sess_decode()
 * to output and input, respectively, base64 encoded data much like
 * what sess_saslc_cont() does according to the SASLC_FLAGS_BASE64_*
 * flags.  For saslc_sess_decode() it seems it would be easiest to do
 * this in saslc__buffer32_fetch() pushing any extra buffering into
 * the BIO_* routines, but I haven't thought this through carefully
 * yet.
 */

static inline char *
skip_WS(char *p)
{

	while (*p == ' ' || *p == '\t')
		p++;
	return p;
}

/**
 * @brief convert a comma and/or space delimited list into a comma
 * delimited list of the form:
 *   ( *LWS element *( *LWS "," *LWS element ))
 * @param str string to convert.
 */
static void
normalize_list_string(char *opts)
{
	char *p;

	p = opts;
	while (p != NULL) {
		p = strchr(p, ' ');
		if (p == NULL)
			break;
		if (p > opts && p[-1] != ',')
			*p++ = ',';
		p = skip_WS(p + 1);
	}
}

/**
 * @brief get the security flags from a comma delimited string.
 * @param sec_opt the security option comman delimited string.
 * @return the flags on success, or -1 on error (no memory).
 */
static int
get_security_flags(const char *sec_opts)
{
	static const named_flag_t flag_tbl[] = {
		{ "noanonymous",	FLAG_ANONYMOUS },
		{ "nodictionary",	FLAG_DICTIONARY },
		{ "noplaintext",	FLAG_PLAINTEXT },
		{ "noactive",		FLAG_ACTIVE },
		{ "mutual",		FLAG_MUTUAL },
		{ NULL,			FLAG_NONE }
	};
	list_t *list;
	char *opts;
	uint32_t flags;
	int rv;

	if (sec_opts == NULL)
		return 0;

	if ((opts = strdup(sec_opts)) == NULL)
		return -1;

	normalize_list_string(opts);
	rv = saslc__list_parse(&list, opts);
	free(opts);
	if (rv == -1)
		return -1;
	flags = saslc__list_flags(list, flag_tbl);
	saslc__list_free(list);
	return flags;
}

/**
 * @brief compare the mechanism flags with the security option flags
 * passed by the user and make sure the mechanism is OK.
 * @param mech mechanism to check.
 * @param flags security option flags passed by saslc_sess_init().
 * @return true if the mechanism is permitted and false if not.
 */
static bool
mechanism_flags_OK(const saslc__mech_list_node_t *mech, uint32_t flags)
{
	uint32_t reqflags, rejflags;

	if (mech == NULL)
		return false;

	reqflags = flags & REQ_FLAGS;
	rejflags = flags & REJ_FLAGS;

	if ((mech->mech->flags & rejflags) != 0)
		return false;

	if ((mech->mech->flags & reqflags) != reqflags)
		return false;

	return true;
}

/**
 * @brief chooses first supported mechanism from the mechs list for
 * the sasl session.
 * @param ctx sasl context
 * @param mechs comma or space separated list of mechanisms
 * e.g., "PLAIN,LOGIN" or "PLAIN LOGIN".
 * @param sec_opts comma or space separated list of security options
 * @return pointer to the mech on success, NULL if none mechanism is chosen
 *
 * Note: this uses SASLC_PROP_SECURITY from the context dictionary.
 * Note: this function is not case sensitive with regard to mechs or sec_opts.
 */
static const saslc__mech_t *
saslc__sess_choose_mech(saslc_t *ctx, const char *mechs, const char *sec_opts)
{
	list_t *list, *l;
	char *tmpstr;
	const saslc__mech_list_node_t *m;
	uint32_t flags;
	int rv;

	rv = get_security_flags(sec_opts);
	if (rv == -1)
		goto nomem;
	flags = rv;

	sec_opts = saslc__dict_get(ctx->prop, SASLC_PROP_SECURITY);
	if (sec_opts != NULL) {
		rv = get_security_flags(sec_opts);
		if (rv == -1)
			goto nomem;
		flags |= rv;
	}
	if ((tmpstr = strdup(mechs)) == NULL)
		goto nomem;

	normalize_list_string(tmpstr);
	rv = saslc__list_parse(&list, tmpstr);
	free(tmpstr);
	if (rv == -1)
		goto nomem;

	m = NULL;
	for (l = list; l != NULL; l = l->next) {
		m = saslc__mech_list_get(ctx->mechanisms, l->value);
		if (mechanism_flags_OK(m, flags))
			break;
	}
	saslc__list_free(list);

	if (m == NULL) {
		saslc__error_set(ERR(ctx), ERROR_MECH,
		    "mechanism not supported");
		return NULL;
	}
	return m->mech;
 nomem:
	saslc__error_set_errno(ERR(ctx), ERROR_NOMEM);
	return NULL;
}

/**
 * @brief sasl session initializaion. Function initializes session
 * property dictionary, chooses best mechanism, creates mech session.
 * @param ctx sasl context
 * @param mechs comma or space separated list of mechanisms eg. "PLAIN,LOGIN"
 * or "PLAIN LOGIN".  Note that this function is not case sensitive.
 * @return pointer to the sasl session on success, NULL on failure
 */
saslc_sess_t *
saslc_sess_init(saslc_t *ctx, const char *mechs, const char *sec_opts)
{
	saslc_sess_t *sess;
	const char *debug;
	saslc__mech_list_node_t *m;

	if ((sess = calloc(1, sizeof(*sess))) == NULL) {
		saslc__error_set_errno(ERR(ctx), ERROR_NOMEM);
		return NULL;
	}

	/* mechanism initialization */
	if ((sess->mech = saslc__sess_choose_mech(ctx, mechs, sec_opts))
	    == NULL)
		goto error;

	/* XXX: special early check of mechanism dictionary for debug flag */
	m = saslc__mech_list_get(ctx->mechanisms, sess->mech->name);
	if (m != NULL) {
		debug = saslc__dict_get(m->prop, SASLC_PROP_DEBUG);
		if (debug != NULL)
			saslc_debug = saslc__parser_is_true(debug);
	}

	/* create mechanism session */
	if (sess->mech->create(sess) == -1)
		goto error;

	/* properties */
	if ((sess->prop = saslc__dict_create()) == NULL) {
		saslc__error_set(ERR(ctx), ERROR_NOMEM, NULL);
		goto error;
	}

	sess->context = ctx;
	ctx->refcnt++;

	saslc__msg_dbg("mechanism: %s\n", saslc_sess_getmech(sess));

	return sess;
 error:
	free(sess);
	return NULL;
}

/**
 * @brief ends sasl session, destroys and deallocates internal
 * resources
 * @param sess sasl session
 */
void
saslc_sess_end(saslc_sess_t *sess)
{

	sess->mech->destroy(sess);
	saslc__dict_destroy(sess->prop);
	sess->context->refcnt--;
	free(sess);
}

/**
 * @brief sets property for the session. If property already exists in
 * the session, then previous value is replaced by the new value.
 * @param sess sasl session
 * @param name property name
 * @param value property value (if NULL, simply remove previous key)
 * @return 0 on success, -1 on failure
 */
int
saslc_sess_setprop(saslc_sess_t *sess, const char *key, const char *value)
{

	/* if the key exists in the session dictionary, remove it */
	(void)saslc__dict_remove(sess->prop, key);

	if (value == NULL)	/* simply remove previous value and return */
		return 0;

	switch (saslc__dict_insert(sess->prop, key, value)) {
	case DICT_OK:
		return 0;

	case DICT_VALBAD:
		saslc__error_set(ERR(sess), ERROR_BADARG, "bad value");
		break;
	case DICT_KEYINVALID:
		saslc__error_set(ERR(sess), ERROR_BADARG, "bad key");
		break;
	case DICT_NOMEM:
		saslc__error_set(ERR(sess), ERROR_NOMEM, NULL);
		break;
	case DICT_KEYEXISTS:
	case DICT_KEYNOTFOUND:
		assert(/*CONSTCOND*/0); /* impossible */
		break;
	}
	return -1;
}

/**
 * @brief gets property from the session. Dictionaries are used
 * in following order: session dictionary, context dictionary (global
 * configuration), mechanism dicionary.
 * @param sess sasl session
 * @param key property name
 * @return property value on success, NULL on failure.
 */
const char *
saslc_sess_getprop(saslc_sess_t *sess, const char *key)
{
	const char *r;
	saslc__mech_list_node_t *m;

	/* get property from the session dictionary */
	if ((r = saslc__dict_get(sess->prop, key)) != NULL) {
		saslc__msg_dbg("%s: session dict: %s=%s", __func__, key, r);
		return r;
	}

	/* get property from the context dictionary */
	if ((r = saslc__dict_get(sess->context->prop, key)) != NULL) {
		saslc__msg_dbg("%s: context dict: %s=%s", __func__, key, r);
		return r;
	}

	/* get property from the mechanism dictionary */
	if ((m = saslc__mech_list_get(sess->context->mechanisms,
	    sess->mech->name)) == NULL)
		return NULL;

	if ((r = saslc__dict_get(m->prop, key)) != NULL)
		saslc__msg_dbg("%s: mech %s dict: %s=%s", __func__,
		    saslc_sess_getmech(sess), key, r);
	else
		saslc__msg_dbg("%s: %s not found", __func__, key);
	return r;
}

/**
 * @brief set the sess->flags accordingly according to the properties.
 * @param sess saslc session
 */
static uint32_t
saslc__sess_get_flags(saslc_sess_t *sess)
{
	const char *base64io;
	uint32_t flags;

	/* set default flags */
	flags = SASLC_FLAGS_DEFAULT;

	base64io = saslc_sess_getprop(sess, SASLC_PROP_BASE64IO);
	if (base64io != NULL) {
		if (saslc__parser_is_true(base64io))
			flags |= SASLC_FLAGS_BASE64;
		else
			flags &= ~SASLC_FLAGS_BASE64;
	}
	return flags;
}

/**
 * @brief does one step of the sasl authentication, input data
 * and its lenght are stored in in and inlen, output is stored in out and
 * outlen. This function is a wrapper for mechanism step functions.
 * Additionaly it checks if session is not already authorized and handles
 * steps mech_sess structure.
 * @param sess saslc session
 * @param in input data
 * @param inlen input data length
 * @param out output data
 * @param outlen output data length
 * @return MECH_OK - on success, no more steps are needed
 * MECH_ERROR - on error, additionaly errno in sess is setup
 * MECH_STEP - more steps are needed
 */
int
saslc_sess_cont(saslc_sess_t *sess, const void *in, size_t inlen,
    void **out, size_t *outlen)
{
	saslc__mech_sess_t *ms;
	const char *debug;
	void *dec;
	int rv;

	ms = sess->mech_sess;
	if (ms->status == STATUS_AUTHENTICATED) {
		saslc__error_set(ERR(sess), ERROR_MECH,
		    "session authenticated");
		return MECH_ERROR;
	}
	if (ms->step == 0) {
		sess->flags = saslc__sess_get_flags(sess);

		/* XXX: final check for any session debug flag setting */
		debug = saslc__dict_get(sess->prop, SASLC_PROP_DEBUG);
		if (debug != NULL)
			saslc_debug = saslc__parser_is_true(debug);
	}

	saslc__msg_dbg("%s: encoded: inlen=%zu in='%s'", __func__, inlen,
	    in ? (const char *)in : "<null>");
	if (inlen == 0 || (sess->flags & SASLC_FLAGS_BASE64_IN) == 0)
		dec = NULL;
	else {
		if (saslc__crypto_decode_base64(in, inlen, &dec, &inlen)
		    == -1) {
			saslc__error_set(ERR(sess), ERROR_MECH,
			    "base64 decode failed");
			return MECH_ERROR;
		}
		in = dec;
	}
	saslc__msg_dbg("%s: decoded: inlen=%zu in='%s'", __func__, inlen,
	    in ? (const char *)in : "<null>");
	rv = sess->mech->cont(sess, in, inlen, out, outlen);
	if (dec != NULL)
		free(dec);
	if (rv == MECH_ERROR)
		return MECH_ERROR;

	saslc__msg_dbg("%s: out='%s'", __func__,
	    *outlen ? (char *)*out : "<null>");
	if (*outlen == 0)
		*out = NULL;	/* XXX: unnecessary? */
	else if ((sess->flags & SASLC_FLAGS_BASE64_OUT) != 0) {
		char *enc;
		size_t enclen;

		if (saslc__crypto_encode_base64(*out, *outlen, &enc, &enclen)
		    == -1) {
			free(*out);
			return MECH_ERROR;
		}
		free(*out);
		*out = enc;
		*outlen = enclen;
	}
	if (rv == MECH_OK)
		ms->status = STATUS_AUTHENTICATED;

	ms->step++;
	return rv;
}

/**
 * @brief copies input data to an allocated buffer.  The caller is
 * responsible for freeing the buffer.
 * @param sess sasl session
 * @param xxcode codec to encode or decode one block of data
 * @param in input data
 * @param inlen input data length
 * @param out output data
 * @param outlen output data length
 * @return number of bytes copied on success, -1 on failure
 */
static ssize_t
saslc__sess_copyout(saslc_sess_t *sess, const void *in, size_t inlen,
    void **out, size_t *outlen)
{

	*out = malloc(inlen);
	if (*out == NULL) {
		*outlen = 0;
		saslc__error_set_errno(ERR(sess), ERROR_NOMEM);
		return -1;
	}
	*outlen = inlen;
	memcpy(*out, in, inlen);
	return inlen;
}

/**
 * @brief encodes or decode data using method established during the
 * authentication. Input data is stored in in and inlen and output
 * data is stored in out and outlen.  The caller is responsible for
 * freeing the output buffer.
 * @param sess sasl session
 * @param xxcode codec to encode or decode one block of data
 * @param in input data
 * @param inlen input data length
 * @param out output data
 * @param outlen output data length
 * @return number of bytes consumed on success, 0 if insufficient data
 * to process, -1 on failure
 *
 * 'xxcode' encodes or decodes a single block of data and stores the
 * resulting block and its length in 'out' and 'outlen', respectively.
 * It should return the number of bytes it digested or -1 on error.
 * If it was unable to process a complete block, it should return zero
 * and remember the partial block internally.  If it is called with
 * 'inlen' = 0, it should flush out any remaining partial block data
 * and return the number of stored bytes it flushed or zero if there
 * were none (relevant for the encoder only).
 */
static ssize_t
saslc__sess_xxcode(saslc_sess_t *sess, saslc__mech_xxcode_t xxcode,
    const void *in, size_t inlen, void **out, size_t *outlen)
{
	saslc__mech_sess_t *ms;
	unsigned char *p;
	void *buf, *pkt;
	size_t buflen, pktlen;
	ssize_t len, ate;

	ms = sess->mech_sess;

	if (xxcode == NULL) {
		saslc__error_set(ERR(sess), ERROR_MECH,
		    "security layer is not supported by mechanism");
		return -1;
	}
	if (ms->status != STATUS_AUTHENTICATED) {
		saslc__error_set(ERR(sess), ERROR_MECH,
		    "session is not authenticated");
		return -1;
	}

	if (ms->qop == QOP_NONE)
		return saslc__sess_copyout(sess, in, inlen, out, outlen);

	p = NULL;
	buf = NULL;
	buflen = 0;
	ate = 0;
	do {
		len = xxcode(sess, in, inlen, &pkt, &pktlen);
		if (len == -1) {
			free(buf);
			return -1;
		}

		ate += len;
		in = (const char *)in + len;
		if (inlen < (size_t)len)
			inlen = 0;
		else
			inlen -= len;

		if (pktlen == 0)	/* nothing processed, done */
			continue;

		buflen += pktlen;
		p = buf;
		if ((buf = realloc(buf, buflen)) == NULL) {
			/* we should free memory if realloc(2) failed */
			free(p);
			saslc__error_set_errno(ERR(sess), ERROR_NOMEM);
			return -1;
		}
		p = buf;
		p += buflen - pktlen;
		memcpy(p, pkt, pktlen);
		free(pkt);
	} while (inlen > 0);

	*out = buf;
	*outlen = buflen;
	return ate;
}

/**
 * @brief encodes data using method established during the
 * authentication. Input data is stored in in and inlen and output
 * data is stored in out and outlen.  The caller is responsible for
 * freeing the output buffer.
 * @param sess sasl session
 * @param in input data
 * @param inlen input data length
 * @param out output data
 * @param outlen output data length
 * @return 0 on success, -1 on failure
 *
 * This will output a sequence of full blocks.  When all data has been
 * processed, this should be called one more time with inlen = 0 to
 * flush any partial block left in the encoder.
 */
ssize_t
saslc_sess_encode(saslc_sess_t *sess, const void *in, size_t inlen,
	void **out, size_t *outlen)
{

	return saslc__sess_xxcode(sess, sess->mech->encode,
	    in, inlen, out, outlen);
}

/**
 * @brief decodes data using method established during the
 * authentication. Input data is stored in in and inlen and output
 * data is stored in out and outlen.  The caller is responsible for
 * freeing the output buffer.
 * @param sess sasl session
 * @param in input data
 * @param inlen input data length
 * @param out output data
 * @param outlen output data length
 * @return 0 on success, -1 on failure
 */
ssize_t
saslc_sess_decode(saslc_sess_t *sess, const void *in, size_t inlen,
    void **out, size_t *outlen)
{

	return saslc__sess_xxcode(sess, sess->mech->decode,
	    in, inlen, out, outlen);
}

/**
 * @brief gets string message of the error.
 * @param sess sasl session
 * @return pointer to the error message
 */
const char *
saslc_sess_strerror(saslc_sess_t *sess)
{

	return saslc__error_get_strerror(ERR(sess));
}

/**
 * @brief gets name of the mechanism used in the sasl session
 * @param sess sasl session
 * @return pointer to the mechanism name
 */
const char *
saslc_sess_getmech(saslc_sess_t *sess)
{

	return sess->mech->name;
}
