/* $NetBSD: mech_crammd5.c,v 1.4 2011/02/12 23:21:32 christos Exp $ */

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
__RCSID("$NetBSD: mech_crammd5.c,v 1.4 2011/02/12 23:21:32 christos Exp $");

#include <sys/param.h>

#include <saslc.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "crypto.h"
#include "mech.h"
#include "saslc_private.h"


/* See RFC 2195. */

/* properties */
#define SASLC_CRAMMD5_AUTHCID		SASLC_PROP_AUTHCID
#define SASLC_CRAMMD5_PASSWD		SASLC_PROP_PASSWD

/**
 * @brief do one step of the sasl authentication
 * @param sess sasl session
 * @param in input data
 * @param inlen input data length
 * @param out place to store output data
 * @param outlen output data length
 * @return MECH_OK - success,
 * MECH_STEP - more steps are needed,
 * MECH_ERROR - error
 */
static int
saslc__mech_crammd5_cont(saslc_sess_t *sess, const void *in, size_t inlen,
    void **out, size_t *outlen)
{
	const char *authcid, *passwd;
	char *digest, *name;
	int len, rv;

	/* in case we are called before getting challenge from server */
	if (inlen == 0) {
		*out = NULL;
		*outlen = 0;
		return MECH_STEP;
	}
	if ((authcid = saslc_sess_getprop(sess, SASLC_CRAMMD5_AUTHCID))
	    == NULL) {
		saslc__error_set(ERR(sess), ERROR_MECH,
		    "authcid is required for an authentication");
		return MECH_ERROR;
	}
	if ((passwd = saslc_sess_getprop(sess, SASLC_CRAMMD5_PASSWD))
	    == NULL) {
		saslc__error_set(ERR(sess), ERROR_MECH,
		    "passwd is required for an authentication");
		return MECH_ERROR;
	}
	digest = saslc__crypto_hmac_md5_hex((const unsigned char *)passwd,
					    strlen(passwd), in, inlen);
	if (digest == NULL) {
		saslc__error_set_errno(ERR(sess), ERROR_NOMEM);
		return MECH_ERROR;
	}
	if ((len = asprintf(&name, "%s %s", authcid, digest)) == -1) {
		saslc__error_set_errno(ERR(sess), ERROR_NOMEM);
		rv = MECH_ERROR;
	}
	else {
		*out = name;
		*outlen = len;	/* don't count the '\0' byte */
		rv = MECH_OK;
	}
	memset(digest, 0, HMAC_MD5_DIGEST_LENGTH);
	free(digest);
	return rv;
}

/* mechanism definition */
const saslc__mech_t saslc__mech_crammd5 = {
	.name	 = "CRAM-MD5",
	.flags	 = FLAG_DICTIONARY,
	.create	 = saslc__mech_generic_create,
	.cont	 = saslc__mech_crammd5_cont,
	.encode	 = NULL,
	.decode	 = NULL,
	.destroy = saslc__mech_generic_destroy
};
