/* $NetBSD: buffer.c,v 1.2 2011/02/12 23:21:32 christos Exp $ */

/* Copyright (c) 2010 The NetBSD Foundation, Inc.
 * All rights reserved.
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
__RCSID("$NetBSD: buffer.c,v 1.2 2011/02/12 23:21:32 christos Exp $");

#include <sys/param.h>		/* for MIN() */

#include <assert.h>
#include <saslc.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "buffer.h"
#include "error.h"
#include "saslc_private.h"

/*
 * XXX: Should we rename saslc__buffer_* and saslc__buffer32_* to
 * something reflecting their encode and decode, resp, context?
 */

/**
 * encode buffer context
 */
struct saslc__buffer_context_t {
	saslc_sess_t *sess;	/* session pointer (for error messages) */
	size_t maxbuf;		/* allocated length of payload buffer (maxbuf) */
	size_t bufneed;		/* bytes needed in payload buffer */

	/* XXX: must be at end */
	uint8_t buf[1];		/* payload buffer */
};

/**
 * decode buffer context
 *
 * the actual packet looks like:
 *
 * struct {
 *    uint8_t size[4];	// length of packet following this (big endian order)
 *    uint8_t payload[];	// variable length payload area
 *    struct {
 *      uint8_t mac_0_9[10];	// truncated MD5_HMAC hash of size and payload
 *      uint8_t version[2];	// always 1 (big endian order)
 *      uint8_t seqnum[4];      // sequence number (big endian order)
 *    } mac __packed;
 * } __packed
 */
struct saslc__buffer32_context_t {
	saslc_sess_t *sess;	/* session pointer (for error messages) */
	size_t szneed;		/* bytes needed in size buffer */
	size_t bufsize;		/* size of payload buffer */
	size_t maxbuf;		/* allocated length of payload buffer */
	size_t bufneed;		/* bytes needed in payload buffer */

	/* XXX: these must be sequential and at the end! */
	uint8_t szbuf[4];	/* size buffer */
	uint8_t buf[1];		/* payload buffer */
} __packed;

/****************************************
 * saslc__buffer_* routines.
 * For fetching unencoded data.
 */

/**
 * @brief destroy a buffer context
 * @param ctx context to destroy
 * @return nothing
 */
void
saslc__buffer_destroy(saslc__buffer_context_t *ctx)
{

	free(ctx);
}

/**
 * @brief create a buffer context
 * @param sess saslc session
 * @param maxbuf maximum buffer size
 * @return buffer context
 */
saslc__buffer_context_t *
saslc__buffer_create(saslc_sess_t *sess, size_t maxbuf)
{
	saslc__buffer_context_t *ctx;
	size_t buflen;

	buflen = sizeof(*ctx) - sizeof(ctx->buf) + maxbuf;
	ctx = malloc(buflen);
	if (ctx == NULL) {
		saslc__error_set_errno(ERR(sess), ERROR_NOMEM);
		return NULL;
	}
	memset(ctx, 0, sizeof(*ctx) - sizeof(ctx->buf));

	ctx->maxbuf = maxbuf;
	ctx->bufneed = ctx->maxbuf;
	ctx->sess = sess;
	return ctx;
}

/**
 * @brief fetch a block of data from the input stream.
 * @param ctx context
 * @param in input buffer
 * @param inlen input buffer length
 * @param out pointer to output buffer
 * @param outlen pointer to output buffer length
 * @return number of bytes consumed by the current call, or -1 on
 * failure.
 *
 * NOTE: Output is buffered, so if the return is success and outlen is
 * zero, then more data is needed to fill the packet.  The internal
 * buffer can be flushed by calling with inlen = 0.
 */
ssize_t
saslc__buffer_fetch(saslc__buffer_context_t *ctx, const uint8_t *in,
    size_t inlen, uint8_t **out, size_t *outlen)
{
	uint8_t *p;
	size_t len;

	if (inlen == 0) {  /* flush internal buffer */
		*outlen = ctx->maxbuf - ctx->bufneed;
		*out = *outlen != 0 ? ctx->buf : NULL;
		ctx->bufneed = ctx->maxbuf;	/* for next call */
		return 0;
	}

	len = 0;
	if (ctx->bufneed > 0) {
		p = ctx->buf + ctx->maxbuf - ctx->bufneed;
		len = MIN(inlen, ctx->bufneed);
		memcpy(p, in, len);
		ctx->bufneed -= len;
		if (ctx->bufneed > 0) {
			*out = NULL;
			*outlen = 0;
			return len;
		}
		*out = ctx->buf;
		*outlen = ctx->maxbuf;
		ctx->bufneed = ctx->maxbuf;	/* for next call */
		return len;
	}
	assert(/*CONSTCOND*/0);		/* should not happen! */
	saslc__error_set(ERR(ctx->sess), ERROR_MECH, "buffer coding error");
	*out = NULL;
	*outlen = 0;
	ctx->bufneed = ctx->maxbuf;	/* for next call */
	return -1;
}

/****************************************
 * saslc__buffer32_* routines.
 * For fetching an encoded packet.
 * The packet is of the form:
 * struct {
 *     uint8_t size[4];		// bytes in payload
 *     uint8_t payload[];	// packet payload (including any trailing HMAC)
 * } __packed;
 */

/**
 * @brief destroy a buffer32 context
 * @param ctx context to destroy
 * @return nothing
 */
void
saslc__buffer32_destroy(saslc__buffer32_context_t *ctx)
{

	free(ctx);
}

/**
 * @brief create a buffer32 context
 * @param sess saslc session
 * @param maxbuf maximum buffer size
 * @return buffer context
 */
saslc__buffer32_context_t *
saslc__buffer32_create(saslc_sess_t *sess, size_t maxbuf)
{
	saslc__buffer32_context_t *ctx;
	size_t buflen;

	buflen = sizeof(*ctx) - sizeof(ctx->buf) + maxbuf;
	ctx = malloc(buflen);
	if (ctx == NULL) {
		saslc__error_set_errno(ERR(sess), ERROR_NOMEM);
		return NULL;
	}
	memset(ctx, 0, sizeof(*ctx) - sizeof(ctx->buf));

	ctx->maxbuf = maxbuf;
	ctx->szneed = sizeof(ctx->szbuf);
	ctx->sess = sess;
	return ctx;
}

/**
 * @brief fetch a block of data from the input stream.  The block is
 * prefixed in the stream by a 4 byte length field (in network byte
 * order).
 * @param ctx context
 * @param in input buffer
 * @param inlen input buffer length
 * @param out pointer to output buffer
 * @param outlen pointer to output buffer length
 * @return number of bytes consumed by the current call on success, 0
 * if more data is needed, or -1 on failure.
 */
ssize_t
saslc__buffer32_fetch(saslc__buffer32_context_t *ctx, const uint8_t *in,
    size_t inlen, uint8_t **out, size_t *outlen)
{
	uint8_t *p;
	size_t ate, len;

	if (inlen == 0) { /* we cannot flush the decode buffer */
		saslc__error_set(ERR(ctx->sess), ERROR_BADARG,
		    "bad inlen: cannot flush decode buffer");
		return -1;
	}
	ate = 0;
	if (ctx->szneed) {
		p = ctx->szbuf + sizeof(ctx->szbuf) - ctx->szneed;
		len = MIN(inlen, ctx->szneed);
		memcpy(p, in, len);
		ctx->szneed -= len;
		ate += len;
		if (ctx->szneed > 0)
			goto need_more;

		ctx->bufsize = be32dec(ctx->szbuf);
		if (ctx->bufsize == 0) {
			saslc__error_set(ERR(ctx->sess), ERROR_MECH,
			    "pack with no payload");
			return -1;
		}
		if (ctx->bufsize > ctx->maxbuf) {
			saslc__error_set(ERR(ctx->sess), ERROR_MECH,
			    "payload longer than maxbuf");
			return -1;
		}
		in += len;
		inlen -= len;
		ctx->bufneed = ctx->bufsize;
	}
	if (ctx->bufneed) {
		p = ctx->buf + ctx->bufsize - ctx->bufneed;
		len = MIN(inlen, ctx->bufneed);
		memcpy(p, in, len);
		ctx->bufneed -= len;
		ate += len;
		if (ctx->bufneed > 0)
			goto need_more;
	}
	ctx->szneed = sizeof(ctx->szbuf);	/* for next call */
	*out = ctx->szbuf;
	*outlen = sizeof(ctx->szbuf) + ctx->bufsize;
	return ate;
 need_more:
	*out = NULL;
	*outlen = 0;
	return ate;
}
