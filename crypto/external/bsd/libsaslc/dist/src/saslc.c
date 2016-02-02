/* $NetBSD: saslc.c,v 1.4 2011/02/12 23:21:32 christos Exp $ */

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
 * PURPOSE ARE DISCLAIMED.      IN NO EVENT SHALL THE FOUNDATION OR CONTRIBUTORS
 * BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 * CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
 * POSSIBILITY OF SUCH DAMAGE.
 */
#include <sys/cdefs.h>
__RCSID("$NetBSD: saslc.c,v 1.4 2011/02/12 23:21:32 christos Exp $");

#include <assert.h>
#include <ctype.h>
#include <saslc.h>
#include <stdbool.h>
#include <stdbool.h>
#include <stdio.h>
#include <string.h>

#include "crypto.h"  /* XXX: for saslc_{de,en}code64() */
#include "dict.h"
#include "error.h"
#include "mech.h"
#include "msg.h"
#include "parser.h"
#include "saslc_private.h"

/**
 * @brief check for a valid application name (no path separator)
 * @param appname application name
 * @return true if application name is valid, false otherwise
 */
static bool
saslc__valid_appname(const char *appname)
{
	const char *p;

	for (p = appname; *p; p++)
		if (*p == '/')
			return false;

	return true;
}

/**
 * @brief allocates new saslc context
 * @return pointer to the saslc context
 */
saslc_t *
saslc_alloc(void)
{

	/* XXX: Check this as early as possible. */
	saslc_debug = getenv(SASLC_ENV_DEBUG) != NULL;

	return calloc(1, sizeof(saslc_t));
}

/**
 * @brief initializes sasl context, basing on application name function
 * parses configuration files, sets up default properties and creates
 * mechanisms list for the context.
 * @param ctx sasl context
 * @param appname application name, NULL could be used for generic aplication
 * @param pathname location of config files. if NULL, use environment or default
 * @return 0 on success, -1 otherwise.
 */
int
saslc_init(saslc_t *ctx, const char *appname, const char *pathname)
{

	/* ctx is already zeroed by saslc_alloc(). */
	ctx->prop = saslc__dict_create();

	if (appname != NULL) {
		if (saslc__valid_appname(appname) == false) {
			saslc__error_set(ERR(ctx), ERROR_BADARG,
			    "application name is not permited");
			goto error;
		}
		if ((ctx->appname = strdup(appname)) == NULL) {
			saslc__error_set_errno(ERR(ctx), ERROR_NOMEM);
			goto error;
		}
	}
	if (pathname != NULL && *pathname != '\0') {
		if ((ctx->pathname = strdup(pathname)) == NULL) {
			saslc__error_set_errno(ERR(ctx), ERROR_NOMEM);
			goto error;
		}
	}
	ctx->mechanisms = saslc__mech_list_create(ctx);
	if (ctx->mechanisms == NULL)
		goto error;

	/* load the global and mechanism dictionaries */
	if (saslc__parser_config(ctx) == -1) {
		free(ctx->appname);
		ctx->appname = NULL;
		saslc__dict_destroy(ctx->prop);
		ctx->prop = NULL;
		saslc__mech_list_destroy(ctx->mechanisms);
		ctx->mechanisms = NULL;
		return -1;
	}
	return 0;

 error:
	if (ctx->pathname != NULL) {
		free(ctx->pathname);
		ctx->pathname = NULL;
	}
	if (ctx->appname != NULL) {
		free(ctx->appname);
		ctx->appname = NULL;
	}
	free(ctx->prop);
	ctx->prop = NULL;
	return -1;
}

/**
 * @brief gets string message of last error.
 * @param ctx context
 * @return pointer to the error message.
 */
const char *
saslc_strerror(saslc_t *ctx)
{

	return saslc__error_get_strerror(ERR(ctx));
}

/**
 * @brief destroys and deallocate resources used by the context.
 * @param ctx context
 * the context (if any) should be destroyed
 * @return 0 on success, -1 on failure
 */
int
saslc_end(saslc_t *ctx)
{

	if (ctx->refcnt > 0) {
		saslc__error_set(ERR(ctx), ERROR_GENERAL,
		    "context has got assigned active sessions");
		return -1;
	}

	if (ctx->mechanisms != NULL)
		saslc__mech_list_destroy(ctx->mechanisms);

	if (ctx->prop != NULL)
		saslc__dict_destroy(ctx->prop);

	free(ctx->appname);
	free(ctx);
	return 0;
}
