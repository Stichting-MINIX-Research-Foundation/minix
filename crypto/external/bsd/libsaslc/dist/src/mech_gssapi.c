/* $NetBSD: mech_gssapi.c,v 1.7 2013/05/16 13:02:12 elric Exp $ */

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
__RCSID("$NetBSD: mech_gssapi.c,v 1.7 2013/05/16 13:02:12 elric Exp $");

#include <assert.h>
#include <errno.h>
#include <limits.h>	/* for LINE_MAX */
#include <saslc.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <gssapi/gssapi.h>

#include "buffer.h"
#include "list.h"
#include "mech.h"
#include "msg.h"
#include "saslc_private.h"

/* See RFC 2222 section 7.2.1. */

/* properties */
#define SASLC_GSSAPI_AUTHCID		SASLC_PROP_AUTHCID
#define SASLC_GSSAPI_HOSTNAME		SASLC_PROP_HOSTNAME
#define SASLC_GSSAPI_SERVICE		SASLC_PROP_SERVICE
#define SASLC_GSSAPI_QOPMASK		SASLC_PROP_QOPMASK

#define DEFAULT_QOP_MASK	(F_QOP_NONE | F_QOP_INT | F_QOP_CONF)

/* authentication steps */
typedef enum {	/* see RFC2222 7.2.1 section */
	GSSAPI_AUTH_FIRST,		/* first authentication stage */
	GSSAPI_AUTH_NEXT,		/* next authentication stage(s) */
	GSSAPI_AUTH_LAST,		/* final authentication stage */
	GSSAPI_AUTH_DONE		/* authenticated */
} saslc__mech_gssapi_status_t;

/* gssapi mechanism session */
typedef struct {
	saslc__mech_sess_t mech_sess;		/* mechanism session */
	saslc__mech_gssapi_status_t status;	/* authentication status */
	gss_ctx_id_t gss_ctx;			/* GSSAPI context */
	gss_name_t server_name;			/* server name: service@host */
	gss_name_t client_name;			/* client name - XXX: unused! */
	uint32_t qop_mask;			/* available QOP services */
	uint32_t omaxbuf;			/* maximum output buffer size */
	uint32_t imaxbuf;			/* maximum input buffer size */
	saslc__buffer32_context_t *dec_ctx;	/* decode buffer context */
	saslc__buffer_context_t *enc_ctx;	/* encode buffer context */
} saslc__mech_gssapi_sess_t;

/**
 * @brief creates gssapi mechanism session.
 * Function initializes also default options for the session.
 * @param sess sasl session
 * @return 0 on success, -1 on failure.
 */
static int
saslc__mech_gssapi_create(saslc_sess_t *sess)
{
	saslc__mech_gssapi_sess_t *c;

	c = sess->mech_sess = calloc(1, sizeof(*c));
	if (c == NULL)
		return -1;

	sess->mech_sess = c;

	c->gss_ctx = GSS_C_NO_CONTEXT;
	c->server_name = GSS_C_NO_NAME;
	c->client_name = GSS_C_NO_NAME;

	return 0;
}

/**
 * @brief destroys gssapi mechanism session.
 * Function also is freeing assigned resources to the session.
 * @param sess sasl session
 * @return Functions always returns 0.
 */
static int
saslc__mech_gssapi_destroy(saslc_sess_t *sess)
{
	saslc__mech_gssapi_sess_t *ms;
	OM_uint32 min_s;

	ms = sess->mech_sess;

	if (ms->gss_ctx != GSS_C_NO_CONTEXT)
		gss_delete_sec_context(&min_s, &ms->gss_ctx, GSS_C_NO_BUFFER);
	if (ms->server_name != GSS_C_NO_NAME)
		gss_release_name(&min_s, &ms->server_name);
	if (ms->client_name != GSS_C_NO_NAME)
		gss_release_name(&min_s, &ms->client_name);

	saslc__buffer_destroy(ms->enc_ctx);
	saslc__buffer32_destroy(ms->dec_ctx);
	free(ms);
	sess->mech_sess = NULL;

	return 0;
}

/**
 * @brief translate the major and minor statuses an error message for
 * the given mechanism
 * @param maj_s major status
 * @param min_s minor status
 * @param mech mechanism
 * @return pointer to a static buffer with error message
 */
static char *
saslc__mech_gssapi_err(OM_uint32 maj_s, OM_uint32 min_s, gss_OID mech)
{
	static char errbuf[LINE_MAX];
	gss_buffer_desc maj_error_message;
	gss_buffer_desc min_error_message;
	OM_uint32 disp_min_s;
	OM_uint32 msg_ctx;

	msg_ctx = 0;
	maj_error_message.length = 0;
	maj_error_message.value = NULL;
	min_error_message.length = 0;
	min_error_message.value = NULL;

	(void)gss_display_status(&disp_min_s, maj_s, GSS_C_GSS_CODE,
	    mech, &msg_ctx, &maj_error_message);
	(void)gss_display_status(&disp_min_s, min_s, GSS_C_MECH_CODE,
	    mech, &msg_ctx, &min_error_message);

	(void)snprintf(errbuf, sizeof(errbuf),
	    "gss-code: %lu %.*s\nmech-code: %lu %.*s",
	    (unsigned long)maj_s,
	    (int)maj_error_message.length,
	    (char *)maj_error_message.value,
	    (unsigned long)min_s,
	    (int)min_error_message.length,
	    (char *)min_error_message.value);

	(void)gss_release_buffer(&disp_min_s, &maj_error_message);
	(void)gss_release_buffer(&disp_min_s, &min_error_message);

	return errbuf;
}

/**
 * @brief set a session error message using saslc__mech_gssapi_err()
 * @param sess the session
 * @param err error number to set
 * @param maj_s major status
 * @param min_s minor status
 * @return pointer to a static buffer with error message
 */
static void
saslc__mech_gssapi_set_err(saslc_sess_t *sess, int err, OM_uint32 maj_s, OM_uint32 min_s)
{

	saslc__error_set(ERR(sess), err,
	    saslc__mech_gssapi_err(maj_s, min_s, GSS_C_NO_OID));
}

/**
 * @brief convert an initialization output token into the out and outlen format.
 * Also releases the output token.
 * @param sess saslc session
 * @param outbuf gss buffer token
 * @param out pointer to a void pointer
 * @param outlen pointer to size_t length storage
 * @returns 0 on success, -1 on failure
 */
static int
prep_output(saslc_sess_t *sess, gss_buffer_t outbuf, void **out, size_t *outlen)
{
	OM_uint32 min_s;

	if (outbuf == GSS_C_NO_BUFFER || outbuf->value == NULL) {
		*outlen = 0;
		*out = NULL;
		return 0;
	}
	if (outbuf->length == 0) {
		*outlen = 0;
		*out = NULL;
		gss_release_buffer(&min_s, outbuf);
		return 0;
	}
	*out = malloc(outbuf->length);
	if (*out == NULL) {
		*outlen = 0;
		gss_release_buffer(&min_s, outbuf);
		saslc__error_set_errno(ERR(sess), ERROR_NOMEM);
		return -1;
	}
	*outlen = outbuf->length;
	memcpy(*out, outbuf->value, outbuf->length);
	gss_release_buffer(&min_s, outbuf);
	return 0;
}

/**
 * @brief convert an output token into a valid packet where the first
 * 4 bytes are the payload length in network byte order.
 * Also releases the output token.
 * @param sess saslc session
 * @param outbuf gss buffer token
 * @param out pointer to a void pointer
 * @param outlen pointer to size_t length storage
 * @returns 0 on success, -1 on failure
 */
static int
prep_packet(saslc_sess_t *sess, gss_buffer_t outbuf, void **out, size_t *outlen)
{
	saslc__mech_gssapi_sess_t *ms;
	OM_uint32 min_s;
	char *buf;
	size_t buflen;

	ms = sess->mech_sess;

	if (outbuf == GSS_C_NO_BUFFER || outbuf->value == NULL) {
		*outlen = 0;
		*out = NULL;
		return 0;
	}
	if (outbuf->length == 0) {
		*outlen = 0;
		*out = NULL;
		gss_release_buffer(&min_s, outbuf);
		return 0;
	}
	buflen = outbuf->length + 4;
	if (buflen > ms->omaxbuf) {
		saslc__error_set(ERR(sess), ERROR_MECH,
		    "output exceeds server maxbuf size");
		gss_release_buffer(&min_s, outbuf);
		return -1;
	}
	buf = malloc(buflen);
	if (buf == NULL) {
		saslc__error_set_errno(ERR(sess), ERROR_NOMEM);
		return -1;
	}
	be32enc(buf, (uint32_t)outbuf->length);
	memcpy(buf + 4, outbuf->value, outbuf->length);
	gss_release_buffer(&min_s, outbuf);

	*out = buf;
	*outlen = buflen;
	return 0;
}

/**
 * @brief encodes one block of data using the negotiated security layer.
 * @param sess sasl session
 * @param in input data
 * @param inlen input data length
 * @param out place to store output data
 * @param outlen output data length
 * @return number of bytes consumed, zero if more needed, or -1 on failure.
 */
static ssize_t
saslc__mech_gssapi_encode(saslc_sess_t *sess, const void *in, size_t inlen,
    void **out, size_t *outlen)
{
	saslc__mech_gssapi_sess_t *ms;
	gss_buffer_desc input, output;
	OM_uint32 min_s, maj_s;
	uint8_t *buf;
	size_t buflen;
	ssize_t len;

	ms = sess->mech_sess;
	assert(ms->mech_sess.qop != QOP_NONE);
	if (ms->mech_sess.qop == QOP_NONE)
		return -1;

	len = saslc__buffer_fetch(ms->enc_ctx, in, inlen, &buf, &buflen);
	if (len == -1)
		return -1;

	if (buflen == 0) {
		*out = NULL;
		*outlen = 0;
		return len;
	}

	input.value = buf;
	input.length = buflen;
	output.value = NULL;
	output.length = 0;

	maj_s = gss_wrap(&min_s, ms->gss_ctx, ms->mech_sess.qop == QOP_CONF,
	    GSS_C_QOP_DEFAULT, &input, NULL, &output);

	if (GSS_ERROR(maj_s)) {
		saslc__mech_gssapi_set_err(sess, ERROR_MECH, maj_s, min_s);
		return -1;
	}
	if (prep_packet(sess, &output, out, outlen) == -1)
		return -1;

	return len;
}

/**
 * @brief decodes one block of data using the negotiated security layer.
 * @param sess sasl session
 * @param in input data
 * @param inlen input data length
 * @param out place to store output data
 * @param outlen output data length
 * @return number of bytes consumed, zero if more needed, or -1 on failure.
 */
static ssize_t
saslc__mech_gssapi_decode(saslc_sess_t *sess, const void *in, size_t inlen,
	void **out, size_t *outlen)
{
	saslc__mech_gssapi_sess_t *ms;
	gss_buffer_desc input, output;
	OM_uint32 min_s, maj_s;
	uint8_t *buf;
	size_t buflen;
	ssize_t len;

	ms = sess->mech_sess;
	assert(ms->mech_sess.qop != QOP_NONE);
	if (ms->mech_sess.qop == QOP_NONE)
		return -1;

	len = saslc__buffer32_fetch(ms->dec_ctx, in, inlen, &buf, &buflen);
	if (len == -1)
		return -1;

	if (buflen == 0) {
		*out = NULL;
		*outlen = 0;
		return len;
	}

	/* buf -> szbuf (4 bytes) followed by the payload buffer */
	input.value = buf + 4;
	input.length = buflen - 4;
	output.value = NULL;
	output.length = 0;

	maj_s = gss_unwrap(&min_s, ms->gss_ctx, &input, &output, NULL, NULL);

	if (GSS_ERROR(maj_s)) {
		saslc__mech_gssapi_set_err(sess, ERROR_MECH, maj_s, min_s);
		return -1;
	}

	if (prep_output(sess, &output, out, outlen) == -1)
		return -1;

	return len;
}

/**
 * @brief get service name from properties
 * ("<servicename>@<hostname>") and store it in service token.
 * @param sess the session context
 * @param service the gs_name_t token to return service name in
 * @return 0 on success, -1 on error
 */
static int
get_service(saslc_sess_t *sess, gss_name_t *service)
{
	gss_buffer_desc bufdesc;
	const char *hostname, *servicename;
	char *buf;
	int buflen;
	OM_uint32 min_s, maj_s;

	hostname = saslc_sess_getprop(sess, SASLC_GSSAPI_HOSTNAME);
	if (hostname == NULL) {
		saslc__error_set(ERR(sess), ERROR_MECH,
		    "hostname is required for an authentication");
		return -1;
	}
	servicename = saslc_sess_getprop(sess, SASLC_GSSAPI_SERVICE);
	if (servicename == NULL) {
		saslc__error_set(ERR(sess), ERROR_MECH,
		    "service is required for an authentication");
		return -1;
	}
	buflen = asprintf(&buf, "%s@%s", servicename, hostname);
	if (buflen == -1) {
		saslc__error_set_errno(ERR(sess), ERROR_NOMEM);
		return -1;
	}
	bufdesc.value = buf;
	bufdesc.length = buflen + 1;

	saslc__msg_dbg("%s: buf='%s'", __func__, buf);

	maj_s = gss_import_name(&min_s, &bufdesc, GSS_C_NT_HOSTBASED_SERVICE,
	    service);
	free(buf);
	if (GSS_ERROR(maj_s)) {
		saslc__mech_gssapi_set_err(sess, ERROR_MECH, maj_s, min_s);
		return -1;
	}
	return 0;
}

/**
 * @brief gss_init_sec_context() wrapper
 * @param sess session context
 * @param inbuf input token
 * @param outbuf output token
 * @return 0 if GSS_S_COMPLETE, 1 if GSS_S_CONTINUE_NEEDED, -1 on failure
 */
static int
init_sec_context(saslc_sess_t *sess, gss_buffer_t inbuf, gss_buffer_t outbuf)
{
	saslc__mech_gssapi_sess_t *ms;
	OM_uint32 min_s, maj_s;

	ms = sess->mech_sess;

	outbuf->length = 0;
	outbuf->value = NULL;
	maj_s = gss_init_sec_context(
		&min_s,			/* minor status */
		GSS_C_NO_CREDENTIAL, /* use current login context credential */
		&ms->gss_ctx,		/* initially GSS_C_NO_CONTEXT */
		ms->server_name,	/* server@hostname */
		GSS_C_NO_OID,		/* use default mechanism */
#if 1
		GSS_C_REPLAY_FLAG |	/* message replay detection */
		GSS_C_INTEG_FLAG |	/* request integrity */
		GSS_C_CONF_FLAG |	/* request confirmation */
#endif
		GSS_C_MUTUAL_FLAG |	/* mutual authentication */
		GSS_C_SEQUENCE_FLAG,	/* message sequence checking */
		0,			/* default lifetime (2 hrs) */
		GSS_C_NO_CHANNEL_BINDINGS,
		inbuf,			/* input token */
		/* output parameters follow */
		NULL,			/* mechanism type for context */
		outbuf,			/* output token */
		NULL,			/* services available for context */
		NULL);			/* lifetime of context */

	switch (maj_s) {
	case GSS_S_COMPLETE:
		return 0;
	case GSS_S_CONTINUE_NEEDED:
		return 1;
	default:
		saslc__mech_gssapi_set_err(sess, ERROR_MECH, maj_s, min_s);
		return -1;
	}
}

/**
 * @brief unwrap the authentication token received from the server.
 * This contains the qop_mask and maxbuf values which are updated in
 * saslc__mech_gssapi_sess_t.
 * @param sess the session context
 * @param inbuf the received authentication token.
 * @return 0 on success, -1 on error.
 */
static int
unwrap_input_token(saslc_sess_t *sess, gss_buffer_t inbuf)
{
	saslc__mech_gssapi_sess_t *ms;
	OM_uint32 min_s, maj_s;
	gss_buffer_t outbuf;
	gss_buffer_desc outdesc;
	unsigned char *p;

	/********************************************************************/
	/* [RFC 2222 section 7.2.1]                                         */
	/* The client passes this token to GSS_Unwrap and interprets        */
	/* the first octet of resulting cleartext as a bit-mask specifying  */
	/* the security layers supported by the server and the second       */
	/* through fourth octets as the maximum size output_message to send */
	/* to the server.                                                   */
	/********************************************************************/

	ms = sess->mech_sess;

	outbuf = &outdesc;
	maj_s = gss_unwrap(&min_s, ms->gss_ctx, inbuf, outbuf, NULL, NULL);

	if (GSS_ERROR(maj_s)) {
		saslc__mech_gssapi_set_err(sess, ERROR_MECH, maj_s, min_s);
		return -1;
	}
	if (outbuf->length != 4) {
		saslc__error_set(ERR(sess), ERROR_MECH,
		    "invalid unwrap length");
		return -1;
	}
	p = outbuf->value;
	ms->qop_mask = p[0];
	ms->omaxbuf = (be32dec(p) & 0xffffff);

	saslc__msg_dbg("%s: qop_mask=0x%02x omaxbuf=%d",
	    __func__, ms->qop_mask, ms->omaxbuf);

	if (ms->qop_mask == QOP_NONE && ms->omaxbuf != 0) {
		saslc__error_set(ERR(sess), ERROR_MECH,
		    "server has no security layer support, but maxbuf != 0");
		return -1;
	}
	maj_s = gss_release_buffer(&min_s, outbuf);
	if (GSS_ERROR(maj_s)) {
		saslc__mech_gssapi_set_err(sess, ERROR_MECH, maj_s, min_s);
		return -1;
	}
	return 0;
}

/**
 * @brief construct and wrap up an authentication token and put it in
 * outbuf.  The outbuf token data is structured as follows:
 * struct {
 *   uint8_t qop;	// qop to use
 *   uint8_t maxbuf[3]	// maxbuf for client (network byte order)
 *   uint8_t authcid[]	// variable length authentication id (username)
 * } __packed;
 * @param sess the session
 * @param outbuf the gss_buffer_t token to return to server.
 * @return 0 on success, -1 on error.
 */
static int
wrap_output_token(saslc_sess_t *sess, gss_buffer_t outbuf)
{
	saslc__mech_gssapi_sess_t *ms;
	gss_buffer_desc indesc;
	char *input_value;
	int len;
	const char *authcid;
	OM_uint32 min_s, maj_s;
	unsigned char *p;

	/********************************************************************/
	/* [RFC 2222 section 7.2.1]                                         */
	/* The client then constructs data, with the first octet containing */
	/* the bit-mask specifying the selected security layer, the second  */
	/* through fourth octets containing in network byte order the       */
	/* maximum size output_message the client is able to receive, and   */
	/* the remaining octets containing the authorization identity.  The */
	/* authorization identity is optional in mechanisms where it is     */
	/* encoded in the exchange such as GSSAPI.  The client passes the   */
	/* data to GSS_Wrap with conf_flag set to FALSE, and responds with  */
	/* the generated output_message.  The client can then consider the  */
	/* server authenticated.                                            */
	/********************************************************************/

	ms = sess->mech_sess;

	authcid = saslc_sess_getprop(sess, SASLC_GSSAPI_AUTHCID);

	len = asprintf(&input_value, "qmax%s", authcid ? authcid : "");
	if (len == -1) {
		saslc__error_set_errno(ERR(sess), ERROR_NOMEM);
		return -1;
	}
	be32enc(input_value, ms->imaxbuf);
	input_value[0] = saslc__mech_qop_flag(ms->mech_sess.qop);

	indesc.value = input_value;
	indesc.length = len;	/* XXX: don't count the '\0' */

	p = (unsigned char *)input_value;
	saslc__msg_dbg("%s: input_value='%02x %02x %02x %02x %s",
	    __func__, p[0], p[1], p[2], p[3], input_value + 4);

	maj_s = gss_wrap(&min_s, ms->gss_ctx, 0 /* FALSE - RFC2222 */,
	    GSS_C_QOP_DEFAULT, &indesc, NULL, outbuf);

	free(input_value);

	if (GSS_ERROR(maj_s)) {
		saslc__mech_gssapi_set_err(sess, ERROR_MECH, maj_s, min_s);
		return -1;
	}
	return 0;
}

/************************************************************************
 * XXX: Share this with mech_digestmd5.c?  They are almost identical.
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

	qop_flags &= DEFAULT_QOP_MASK;
	user_qop = saslc_sess_getprop(sess, SASLC_GSSAPI_QOPMASK);
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
 * @brief compute the maximum buffer length we can use and not
 * overflow the servers maxbuf.
 * @param sess the session context
 * @param maxbuf the server's maxbuf value
 */
static int
wrap_size_limit(saslc_sess_t *sess, OM_uint32 maxbuf)
{
	saslc__mech_gssapi_sess_t *ms;
	OM_uint32 min_s, maj_s;
	OM_uint32 max_input;

	ms = sess->mech_sess;

	maj_s = gss_wrap_size_limit(&min_s, ms->gss_ctx, 1, GSS_C_QOP_DEFAULT,
	    maxbuf, &max_input);

	if (GSS_ERROR(maj_s)) {
		saslc__mech_gssapi_set_err(sess, ERROR_MECH, maj_s, min_s);
		return -1;
	}

	/* XXX: from cyrus-sasl: gssapi.c */
	if (max_input > maxbuf) {
		/* Heimdal appears to get this wrong */
		maxbuf -= (max_input - maxbuf);
	} else {
		/* This code is actually correct */
		maxbuf = max_input;
	}
	return maxbuf;
}

/**
 * @brief set our imaxbuf (from omaxbuf or from properties) and
 * then reset omaxbuf in saslc__mech_gssapi_sess_t.
 * @param sess the session context
 * @return 0 on success, -1 on error
 *
 * Note: on entry the omaxbuf is the server's maxbuf size.  On exit
 * the omaxbuf is the maximum buffer we can fill that will not
 * overflow the servers maxbuf after it is encoded.  This value is
 * given by wrap_size_limit().
 */
static int
set_maxbufs(saslc_sess_t *sess)
{
	saslc__mech_gssapi_sess_t *ms;
	const char *p;
	char *q;
	unsigned long val;
	int rv;

	ms = sess->mech_sess;

	/* by default, we use the same input maxbuf as the server. */
	ms->imaxbuf = ms->omaxbuf;
	p = saslc_sess_getprop(sess, SASLC_PROP_MAXBUF);
	if (p != NULL) {
		val = strtol(p, &q, 0);
		if (p[0] == '\0' || *q != '\0') {

			return MECH_ERROR;
		}
		if (errno == ERANGE && val == ULONG_MAX) {

			return MECH_ERROR;
		}
		if (val > 0xffffff)
			val = 0xffffff;
		ms->imaxbuf = (uint32_t)val;
	}
	rv = wrap_size_limit(sess, ms->omaxbuf);
	if (rv == -1)
		return MECH_ERROR;
	ms->omaxbuf = rv;	/* maxbuf size for unencoded output data */

	return 0;
}

/**
 * @brief do one step of the sasl authentication
 * @param sess sasl session
 * @param in input data
 * @param inlen input data length
 * @param out place to store output data
 * @param outlen output data length
 * @return MECH_OK on success, MECH_STEP if more steps are needed,
 * MECH_ERROR on failure
 */
static int
saslc__mech_gssapi_cont(saslc_sess_t *sess, const void *in, size_t inlen,
    void **out, size_t *outlen)
{
	saslc__mech_gssapi_sess_t *ms;
	gss_buffer_desc input, output;
	int rv;

    /**************************************************************************/
    /* [RFC 2222 section 7.2.1]                                               */
    /* The client calls GSS_Init_sec_context, passing in 0 for                */
    /* input_context_handle (initially) and a targ_name equal to output_name  */
    /* from GSS_Import_Name called with input_name_type of                    */
    /* GSS_C_NT_HOSTBASED_SERVICE and input_name_string of                    */
    /* "service@hostname" where "service" is the service name specified in    */
    /* the protocol's profile, and "hostname" is the fully qualified host     */
    /* name of the server.  The client then responds with the resulting       */
    /* output_token.  If GSS_Init_sec_context returns GSS_S_CONTINUE_NEEDED,  */
    /* then the client should expect the server to issue a token in a         */
    /* subsequent challenge.  The client must pass the token to another call  */
    /* to GSS_Init_sec_context, repeating the actions in this paragraph.      */
    /*                                                                        */
    /* When GSS_Init_sec_context returns GSS_S_COMPLETE, the client takes     */
    /* the following actions: If the last call to GSS_Init_sec_context        */
    /* returned an output_token, then the client responds with the            */
    /* output_token, otherwise the client responds with no data.  The client  */
    /* should then expect the server to issue a token in a subsequent         */
    /* challenge.  The client passes this token to GSS_Unwrap and interprets  */
    /* the first octet of resulting cleartext as a bit-mask specifying the    */
    /* security layers supported by the server and the second through fourth  */
    /* octets as the maximum size output_message to send to the server.  The  */
    /* client then constructs data, with the first octet containing the       */
    /* bit-mask specifying the selected security layer, the second through    */
    /* fourth octets containing in network byte order the maximum size        */
    /* output_message the client is able to receive, and the remaining        */
    /* octets containing the authorization identity.  The client passes the   */
    /* data to GSS_Wrap with conf_flag set to FALSE, and responds with the    */
    /* generated output_message.  The client can then consider the server     */
    /* authenticated.                                                         */
    /**************************************************************************/

	ms = sess->mech_sess;

	switch(ms->status) {
	case GSSAPI_AUTH_FIRST:
		saslc__msg_dbg("%s: status: %s", __func__, "GSSAPI_AUTH_FIRST");

		if (get_service(sess, &ms->server_name) == -1)
			return MECH_ERROR;

		rv = init_sec_context(sess, GSS_C_NO_BUFFER, &output);
		if (rv == -1)
			return MECH_ERROR;

		if (prep_output(sess, &output, out, outlen) == -1)
			return MECH_ERROR;

		ms->status = rv == 0 ? GSSAPI_AUTH_LAST : GSSAPI_AUTH_NEXT;
		return MECH_STEP;

	case GSSAPI_AUTH_NEXT:
		saslc__msg_dbg("%s: status: %s", __func__, "GSSAPI_AUTH_NEXT");

		input.value = __UNCONST(in);
		input.length = inlen;
		if ((rv = init_sec_context(sess, &input, &output)) == -1)
			return MECH_ERROR;

		if (prep_output(sess, &output, out, outlen) == -1)
			return MECH_ERROR;

		if (rv == 0)
			ms->status = GSSAPI_AUTH_LAST;
		return MECH_STEP;

	case GSSAPI_AUTH_LAST:
		saslc__msg_dbg("%s: status: %s", __func__, "GSSAPI_AUTH_LAST");

		input.value = __UNCONST(in);
		input.length = inlen;
		if (unwrap_input_token(sess, &input) == -1)
			return MECH_ERROR;

		if ((rv = choose_qop(sess, ms->qop_mask)) == -1)
			return MECH_ERROR;

		ms->mech_sess.qop = rv;

		if (ms->mech_sess.qop != QOP_NONE) {
			if (ms->mech_sess.qop == QOP_CONF) {
				/*
				 * XXX: where do we negotiate the cipher,
				 *  or do we?
				 */
			}
			if (set_maxbufs(sess) == -1)
				return MECH_ERROR;
			ms->dec_ctx = saslc__buffer32_create(sess, ms->imaxbuf);
			ms->enc_ctx = saslc__buffer_create(sess, ms->omaxbuf);
		}
		if (wrap_output_token(sess, &output) == -1)
			return MECH_ERROR;

		if (prep_output(sess, &output, out, outlen) == -1)
			return MECH_ERROR;

		ms->status = GSSAPI_AUTH_DONE;
		return MECH_OK;

	case GSSAPI_AUTH_DONE:
		assert(/*CONSTCOND*/0);	/* XXX: impossible */
		saslc__error_set(ERR(sess), ERROR_MECH,
		    "already authenticated");
		return MECH_ERROR;

#if 0	/* no default so the compiler can tell us if we miss an enum */
	default:
		assert(/*CONSTCOND*/0); /* impossible */
		/*NOTREACHED*/
#endif
	}
	/*LINTED*/
	assert(/*CONSTCOND*/0);		/* XXX: impossible */
	return MECH_ERROR;
}

/* mechanism definition */
const saslc__mech_t saslc__mech_gssapi = {
	.name	 = "GSSAPI",
	.flags	 = FLAG_NONE,
	.create	 = saslc__mech_gssapi_create,
	.cont	 = saslc__mech_gssapi_cont,
	.encode	 = saslc__mech_gssapi_encode,
	.decode	 = saslc__mech_gssapi_decode,
	.destroy = saslc__mech_gssapi_destroy
};
