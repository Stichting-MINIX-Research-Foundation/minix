/* $NetBSD: mech.c,v 1.7 2011/02/22 05:45:05 joerg Exp $ */

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
__RCSID("$NetBSD: mech.c,v 1.7 2011/02/22 05:45:05 joerg Exp $");

#include <sys/queue.h>

#include <saslc.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "dict.h"
#include "error.h"
#include "mech.h"
#include "msg.h"
#include "saslc_private.h"

/* mechanisms */
__weakref_visible const saslc__mech_t weak_saslc__mech_anonymous
    __weak_reference(saslc__mech_anonymous);
__weakref_visible const saslc__mech_t weak_saslc__mech_crammd5
    __weak_reference(saslc__mech_crammd5);
__weakref_visible const saslc__mech_t weak_saslc__mech_digestmd5
    __weak_reference(saslc__mech_digestmd5);
__weakref_visible const saslc__mech_t weak_saslc__mech_external
    __weak_reference(saslc__mech_external);
__weakref_visible const saslc__mech_t weak_saslc__mech_gssapi
    __weak_reference(saslc__mech_gssapi);
__weakref_visible const saslc__mech_t weak_saslc__mech_login
    __weak_reference(saslc__mech_login);
__weakref_visible const saslc__mech_t weak_saslc__mech_plain
    __weak_reference(saslc__mech_plain);

static const saslc__mech_t *saslc__mechanisms[] = {
	&weak_saslc__mech_anonymous,
	&weak_saslc__mech_crammd5,
	&weak_saslc__mech_digestmd5,
	&weak_saslc__mech_external,
	&weak_saslc__mech_gssapi,
	&weak_saslc__mech_login,
	&weak_saslc__mech_plain,
};

/*
 * This table is used by the inline functions in mech.h, which are
 * used in mech_digestmd5.c and mech_gssapi.c.
 *
 * NB: This is indexed by saslc__mech_sess_qop_t values and must be
 * NULL terminated for use in saslc__list_flags().
 */
const named_flag_t saslc__mech_qop_tbl[] = {
	{ "auth",	F_QOP_NONE },
	{ "auth-int",	F_QOP_INT },
	{ "auth-conf",	F_QOP_CONF },
	{ NULL,		0 }
};

/**
 * @brief creates a list of supported mechanisms and their resources.
 * @param ctx context
 * @return pointer to head of the list, NULL if allocation failed
 */
saslc__mech_list_t *
saslc__mech_list_create(saslc_t *ctx)
{
	saslc__mech_list_t *head = NULL;
	saslc__mech_list_node_t *node = NULL;
	size_t i;

	if ((head = calloc(1, sizeof(*head))) == NULL) {
		saslc__error_set_errno(ERR(ctx), ERROR_NOMEM);
		return NULL;
	}
	for (i = 0; i < __arraycount(saslc__mechanisms); i++) {
		if (saslc__mechanisms[i] == NULL)
			continue;
		if ((node = calloc(1, sizeof(*node))) == NULL)
			goto error;

		if ((node->prop = saslc__dict_create()) == NULL) {
			free(node);
			goto error;
		}

		node->mech = saslc__mechanisms[i];
		LIST_INSERT_HEAD(head, node, nodes);
	}
	return head;

 error:
	saslc__error_set_errno(ERR(ctx), ERROR_NOMEM);
	saslc__mech_list_destroy(head);
	return NULL;
}

/**
 * @brief gets mechanism from the list using name
 * @param list mechanisms list
 * @param mech_name mechanism name
 * @return pointer to the mechanism, NULL if mechanism was not found
 */
saslc__mech_list_node_t *
saslc__mech_list_get(saslc__mech_list_t *list, const char *mech_name)
{
	saslc__mech_list_node_t *node;

	LIST_FOREACH(node, list, nodes) {
		if (strcasecmp(node->mech->name, mech_name) == 0)
			return node;
	}
	return NULL;
}

/**
 * @brief destroys and deallocates mechanism list
 * @param list mechanisms list
 */
void
saslc__mech_list_destroy(saslc__mech_list_t *list)
{
	saslc__mech_list_node_t *node;

	while (!LIST_EMPTY(list)) {
		node = LIST_FIRST(list);
		LIST_REMOVE(node, nodes);
		saslc__dict_destroy(node->prop);
		free(node);
	}
	free(list);
}

/**
 * @brief doing copy of the session property, on error sets
 * error message for the session. Copied data is located in *out and *outlen
 * @param sess sasl session
 * @param out out buffer for the session property copy
 * @param outlen length of the session property copy
 * @param name name of the property
 * @param error_msg error messages set on failure
 * @return MECH_OK - on success
 * MECH_ERROR - on failure
 */
int
saslc__mech_strdup(saslc_sess_t *sess, char **out, size_t *outlen,
    const char *name, const char *error_msg)
{
	const char *value; /* property value */

	/* get value */
	if ((value = saslc_sess_getprop(sess, name)) == NULL) {
		saslc__error_set(ERR(sess), ERROR_MECH, error_msg);
		return MECH_ERROR;
	}
	saslc__msg_dbg("saslc__mech_strdup: value='%s'\n", value);

	/* copy value */
	if ((*out = strdup(value)) == NULL) {
		saslc__error_set_errno(ERR(sess), ERROR_NOMEM);
		return MECH_ERROR;
	}
	if (outlen != NULL)
		*outlen = strlen(value);

	return MECH_OK;
}

/**
 * @brief generic session create function, this
 * function is suitable for the most mechanisms.
 * @return 0 on success, -1 on failure
 */
int
saslc__mech_generic_create(saslc_sess_t *sess)
{
	saslc__mech_sess_t *ms;

	if ((ms = calloc(1, sizeof(*ms))) == NULL) {
		saslc__error_set(ERR(sess), ERROR_NOMEM, NULL);
		return -1;
	}
	sess->mech_sess = ms;
	return 0;
}

/**
 * @brief generic session destroy function, this
 * function is suitable for the most mechanisms.
 * @return function always returns 0
 */
int
saslc__mech_generic_destroy(saslc_sess_t *sess)
{

	free(sess->mech_sess);
	sess->mech_sess = NULL;
	return 0;
}
