/*	$NetBSD: prop_ingest.c,v 1.5 2014/09/05 05:19:24 matt Exp $	*/

/*-
 * Copyright (c) 2006 The NetBSD Foundation, Inc.
 * All rights reserved.
 *
 * This code is derived from software contributed to The NetBSD Foundation
 * by Jason R. Thorpe.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
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

#include "prop_object_impl.h"
#include <prop/proplib.h>

struct _prop_ingest_context {
	prop_ingest_error_t	pic_error;
	prop_type_t		pic_type;
	const char *		pic_key;
	void *			pic_private;
};

/*
 * prop_ingest_context_alloc --
 *	Allocate and initialize an ingest context.
 */
prop_ingest_context_t
prop_ingest_context_alloc(void *xprivate)
{
	prop_ingest_context_t ctx;

	ctx = _PROP_MALLOC(sizeof(*ctx), M_TEMP);
	if (ctx != NULL) {
		ctx->pic_error = PROP_INGEST_ERROR_NO_ERROR;
		ctx->pic_type = PROP_TYPE_UNKNOWN;
		ctx->pic_key = NULL;
		ctx->pic_private = xprivate;
	}
	return (ctx);
}

/*
 * prop_ingest_context_free --
 *	Free an ingest context.
 */
void
prop_ingest_context_free(prop_ingest_context_t ctx)
{

	_PROP_FREE(ctx, M_TEMP);
}

/*
 * prop_ingest_context_error --
 *	Get the error code from an ingest context.
 */
prop_ingest_error_t
prop_ingest_context_error(prop_ingest_context_t ctx)
{

	return (ctx->pic_error);
}

/*
 * prop_ingest_context_type --
 *	Return the type of last object visisted by an ingest context.
 */
prop_type_t
prop_ingest_context_type(prop_ingest_context_t ctx)
{

	return (ctx->pic_type);
}

/*
 * prop_ingest_context_key --
 *	Return the last key looked up by an ingest context.
 */
const char *
prop_ingest_context_key(prop_ingest_context_t ctx)
{

	return (ctx->pic_key);
}

/*
 * prop_ingest_context_private --
 *	Return the caller-private data associated with an ingest context.
 */
void *
prop_ingest_context_private(prop_ingest_context_t ctx)
{

	return (ctx->pic_private);
}

/*
 * prop_dictionary_ingest --
 *	Ingest a dictionary using handlers for each object to translate
 *	into an arbitrary binary format.
 */
bool
prop_dictionary_ingest(prop_dictionary_t dict,
		       const prop_ingest_table_entry rules[],
		       prop_ingest_context_t ctx)
{
	const prop_ingest_table_entry *pite;
	prop_object_t obj;

	ctx->pic_error = PROP_INGEST_ERROR_NO_ERROR;

	for (pite = rules; pite->pite_key != NULL; pite++) {
		ctx->pic_key = pite->pite_key;
		obj = prop_dictionary_get(dict, pite->pite_key);
		ctx->pic_type = prop_object_type(obj);
		if (obj == NULL) {
			if (pite->pite_flags & PROP_INGEST_FLAG_OPTIONAL) {
				if ((*pite->pite_handler)(ctx, NULL) == false) {
					ctx->pic_error =
					    PROP_INGEST_ERROR_HANDLER_FAILED;
					return (false);
				}
				continue;
			}
			ctx->pic_error = PROP_INGEST_ERROR_NO_KEY;
			return (false);
		}
		if (ctx->pic_type != pite->pite_type &&
		    pite->pite_type != PROP_TYPE_UNKNOWN) {
			ctx->pic_error = PROP_INGEST_ERROR_WRONG_TYPE;
			return (false);
		}
		if ((*pite->pite_handler)(ctx, obj) == false) {
			ctx->pic_error = PROP_INGEST_ERROR_HANDLER_FAILED;
			return (false);
		}
	}

	return (true);
}
