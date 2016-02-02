/* $NetBSD: mech_plain.c,v 1.4 2011/02/12 23:21:32 christos Exp $ */

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
__RCSID("$NetBSD: mech_plain.c,v 1.4 2011/02/12 23:21:32 christos Exp $");

#include <saslc.h>
#include <stdio.h>
#include <string.h>

#include "error.h"
#include "mech.h"
#include "msg.h"
#include "saslc_private.h"


/* See RFC 2595. */

/* properties */
#define SASLC_PLAIN_AUTHCID	SASLC_PROP_AUTHCID	/* username key */
#define SASLC_PLAIN_AUTHZID	SASLC_PROP_AUTHZID	/* authorization id */
#define SASLC_PLAIN_PASSWD	SASLC_PROP_PASSWD	/* password key */

#define NUL_DELIM	'\x00'
#define CRED_MAX_LEN	255

/**
 * @brief doing one step of the sasl authentication
 * @param sess sasl session
 * @param in input data
 * @param inlen input data length
 * @param out place to store output data
 * @param outlen output data length
 * @return MECH_OK - success,
 * MECH_STEP - more steps are needed,
 * MECH_ERROR - error
 */
/*ARGSUSED*/
static int
saslc__mech_plain_cont(saslc_sess_t *sess, const void *in __unused,
    size_t inlen __unused, void **out, size_t *outlen)
{
	const char *authzid, *authcid, *passwd;
	char *outstr;
	int len;

	authzid = saslc_sess_getprop(sess, SASLC_PLAIN_AUTHZID);
	if (authzid != NULL && strlen(authzid) > CRED_MAX_LEN) {
		saslc__error_set(ERR(sess), ERROR_MECH,
		    "authzid should be shorter than 256 characters");
		return MECH_ERROR;
	}

	if ((authcid = saslc_sess_getprop(sess, SASLC_PLAIN_AUTHCID))
	    == NULL) {
		saslc__error_set(ERR(sess), ERROR_MECH,
			"authcid is required for an authentication");
		return MECH_ERROR;
	}
	if (strlen(authcid) > CRED_MAX_LEN) {
		saslc__error_set(ERR(sess), ERROR_MECH,
		    "authcid should be shorter than 256 characters");
		return MECH_ERROR;
	}

	if ((passwd = saslc_sess_getprop(sess, SASLC_PLAIN_PASSWD))
	    == NULL) {
		saslc__error_set(ERR(sess), ERROR_MECH,
			"passwd is required for an authentication");
		return MECH_ERROR;
	}
	if (strlen(passwd) > CRED_MAX_LEN) {
		saslc__error_set(ERR(sess), ERROR_MECH,
		    "passwd should be shorter than 256 characters");
		return MECH_ERROR;
	}

	len = asprintf(&outstr, "%s%c%s%c%s", authzid != NULL ? authzid : "",
	    NUL_DELIM, authcid, NUL_DELIM, passwd);
	if (len == -1) {
		saslc__error_set_errno(ERR(sess), ERROR_NOMEM);
		return MECH_ERROR;
	}
	*out = outstr;
	*outlen = len;

	saslc__msg_dbg("saslc__mech_plain_cont: "
	    "authzid='%s' authcid='%s' passwd='%s'\n",
	    authzid != NULL ? authzid : "", authcid, passwd);

	return MECH_OK;
}

/* mechanism definition */
const saslc__mech_t saslc__mech_plain = {
	.name	 = "PLAIN",
	.flags	 = FLAG_PLAINTEXT,
	.create	 = saslc__mech_generic_create,
	.cont	 = saslc__mech_plain_cont,
	.encode	 = NULL,
	.decode	 = NULL,
	.destroy = saslc__mech_generic_destroy
};
