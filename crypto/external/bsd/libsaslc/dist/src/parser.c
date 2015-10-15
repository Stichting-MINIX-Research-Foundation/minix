/* $NetBSD: parser.c,v 1.5 2015/08/08 12:34:33 shm Exp $ */

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
 *  	  This product includes software developed by the NetBSD
 *  	  Foundation, Inc. and its contributors.
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
__RCSID("$NetBSD: parser.c,v 1.5 2015/08/08 12:34:33 shm Exp $");

#include <sys/stat.h>
#include <sys/syslimits.h>	/* for PATH_MAX */

#include <ctype.h>
#include <err.h>
#include <errno.h>
#include <saslc.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "dict.h"
#include "msg.h"
#include "parser.h"
#include "saslc_private.h"

#define SASLC__COMMENT_CHAR	'#'

/* config file location defines */
#define SASLC__CONFIG_PATH		"/etc/saslc.d"
#define SASLC__CONFIG_MAIN_FILE		"saslc"
#define SASLC__CONFIG_MECH_DIRECTORY	"mech"
#define SASLC__CONFIG_SUFFIX		".conf"
#define SASLC__DEFAULT_APPNAME		"saslc"

/* token types */
enum {
	TOKEN_KEY,		/* option (key) */
	TOKEN_STRING,		/* quoted string */
	TOKEN_NUM,		/* number */
	TOKEN_COMMENT,		/* comment character */
	TOKEN_UNKNOWN		/* unknown */
};

/* token structure */
typedef struct saslc__token_t {
	int type;		/**< token type */
	char *val;		/**< token string value */
} saslc__token_t;

static inline char *
skip_WS(char *p)
{

	while (*p == ' ' || *p == '\t')
		p++;
	return p;
}

/**
 * @brief gets token from string c and updates pointer position.
 * @param c pointer to string
 * @return token on success, NULL on failure (e.g. at end of string).
 * On success, c is updated to point on next token.  It's position is
 * undefined on failure.
 *
 * Note: A legal key begins with an isalpha(3) character and is
 * followed by isalnum(3) or '_' characters.
 */
static saslc__token_t *
saslc__parse_get_token(char **c)
{
	saslc__token_t *token;
	char *e;

	*c = skip_WS(*c);
	if (**c == '\0')
		return NULL;

	if ((token = calloc(1, sizeof(*token))) == NULL)
		return NULL;

	token->val = *c;

	if (**c == SASLC__COMMENT_CHAR)
		token->type = TOKEN_COMMENT;

	else if (**c == '\"')
		token->type = TOKEN_STRING;

	else if (isdigit((unsigned char)**c))
		token->type = TOKEN_NUM;

	else if (isalpha((unsigned char)**c))
		token->type = TOKEN_KEY;

	else
		token->type = TOKEN_UNKNOWN;

	switch (token->type) {
	case TOKEN_COMMENT:
		break;
	case TOKEN_NUM:
		errno = 0;
		(void)strtoll(*c, &e, 0);
		if (errno != 0)
			goto err;
		*c = e;
		break;
	case TOKEN_KEY:
		(*c)++;
		while (isalnum((unsigned char)**c) || **c == '_')
			(*c)++;
		break;
	case TOKEN_STRING:
		token->val++;	/* skip initial '\"' */
		(*c)++;
		/*
		 * XXX: should we allow escapes inside the string?
		 */
		while (**c != '\0' && **c != '\"')
			(*c)++;
		if (**c != '\"')
			goto err;
		**c = '\0';	/* kill trailing '\"' */
		(*c)++;
		break;
	case TOKEN_UNKNOWN:
		goto err;
	}

	if (isspace((unsigned char)**c))
		*(*c)++ = '\0';
	else if (**c == SASLC__COMMENT_CHAR)
		**c = '\0';
	else if (**c != '\0')
		goto err;

	return token;
 err:
	free(token);
	return NULL;
}

/**
 * @brief parses line and store result in dict.
 * @param line input line
 * @param dict dictionary in which parsed options will be stored
 * @return 0 on success, -1 on failure.
 */
static int
saslc__parse_line(char *line, saslc__dict_t *dict)
{
	saslc__dict_result_t rv;
	saslc__token_t *t;
	char *key;

	key = NULL;
	while ((t = saslc__parse_get_token(&line)) != NULL) {
		if (t->type == TOKEN_COMMENT) {
			free(t);
			break;
		}

		if (key == NULL) {  /* get the key */
			if (t->type != TOKEN_KEY)
				goto err;
			key = t->val;
		}
		else {  /* get the value and insert in dictionary */
			if (t->type != TOKEN_STRING && t->type != TOKEN_NUM)
				goto err;
			rv = saslc__dict_insert(dict, key, t->val);
			if (rv != DICT_OK && rv != DICT_KEYEXISTS)
				goto err;
			key = NULL;
		}
		free(t);
	}
	if (*line != '\0')	/* processed entire line */
		return -1;
	if (key != NULL)	/* completed key/value cycle */
		return -1;
	return 0;
 err:
	free(t);
	return -1;
}

/**
 * @brief parses file and store result in dict
 * @param ctx saslc context
 * @param path path to the file
 * @param dict dictionary in which parsed options will be stored
 * @return 0 on success, -1 on failure.
 */
static int
saslc__parse_file(saslc_t *ctx, char *path, saslc__dict_t *dict)
{
	FILE *fp;
	char *buf, *lbuf;
	size_t len;
	int rv;

	if ((fp = fopen(path, "r")) == NULL) {
		/* Don't fail if we can't open the file. */
		saslc__msg_dbg("%s: fopen: %s: %s", __func__, path,
		    strerror(errno));
		return 0;
	}
	saslc__msg_dbg("%s: parsing: \"%s\"", __func__, path);
	rv = 0;
	lbuf = NULL;
	while ((buf = fgetln(fp, &len)) != NULL) {
		if (buf[len - 1] == '\n')
			buf[len - 1] = '\0';
		else {
			if ((lbuf = malloc(len + 1)) == NULL) {
				saslc__error_set(ERR(ctx), ERROR_NOMEM, NULL);
				rv = -1;
				break;
			}
			memcpy(lbuf, buf, len);
			lbuf[len] = '\0';
			buf = lbuf;
		}
		if (saslc__parse_line(buf, dict) == -1) {
			saslc__error_set(ERR(ctx), ERROR_PARSE,
			    "can't parse file");
			rv = -1;
			break;
		}
		if (lbuf != NULL) {
			free(lbuf);
			lbuf = NULL;
		}
	}
	if (lbuf != NULL)
		free(lbuf);

	fclose(fp);
	return rv;
}

/**
 * @brief determine if a string indicates true or not.
 * @return true if the string is "true", "yes", or any nonzero
 * integer; false otherwise.
 *
 * XXX: does this really belong here?  Used in parser.c and xsess.c.
 */
bool
saslc__parser_is_true(const char *str)
{
	static const char *true_str[] = {
		"true",
		"yes"
	};
	char *e;
	size_t i;
	long int val;

	if (str == NULL)
		return false;

	val = strtol(str, &e, 0);
	if (*str != '\0' && *e == '\0')
		return val != 0;

	for (i = 0; i < __arraycount(true_str); i++)
		if (strcasecmp(str, true_str[i]) == 0)
			return true;

	return false;
}

/**
 * @brief parse configuration files. By default function reads
 * files from /etc/saslc.d/saslc/ directory if appname is not setup. Otherwise
 * function uses /etc/saslc.d/[appname]/ directory. /etc/saslc.d/ is default
 * directory which stores configuration for all applications, but can be
 * overwritten by SASLC_CONFIG variable in environment.
 * @param ctx saslc context
 * @return 0 on success, -1 on failure.
 */
int
saslc__parser_config(saslc_t *ctx)
{
	char path[PATH_MAX + 1];
	struct stat sb;
	saslc__mech_list_node_t *mech_node;
	const char *config_path, *debug, *appname;

	config_path = ctx->pathname;
	if (config_path == NULL)
		config_path = getenv(SASLC_ENV_CONFIG);
	if (config_path == NULL)
		config_path = SASLC__CONFIG_PATH;

	if (stat(config_path, &sb) == -1 || !S_ISDIR(sb.st_mode)) {
		/* XXX: should this be fatal or silently ignored? */
		saslc__msg_err("%s: stat: config_path='%s': %s", __func__,
		    config_path, strerror(errno));
		return 0;
	}

	if ((appname = ctx->appname) == NULL)
		appname = SASLC__DEFAULT_APPNAME;

	/* parse global config file */
	snprintf(path, sizeof(path), "%s/%s/%s%s", config_path,
	    appname, SASLC__CONFIG_MAIN_FILE, SASLC__CONFIG_SUFFIX);
	if (saslc__parse_file(ctx, path, ctx->prop) == -1)
		return -1;

	/* XXX: check this as early as possible! */
	debug = saslc__dict_get(ctx->prop, SASLC_PROP_DEBUG);
	if (debug != NULL)
		saslc_debug = saslc__parser_is_true(debug);

	/* parse mechanism config files */
	LIST_FOREACH(mech_node, ctx->mechanisms, nodes) {
		snprintf(path, sizeof(path), "%s/%s/%s/%s%s",
		    config_path, appname, SASLC__CONFIG_MECH_DIRECTORY,
		    mech_node->mech->name, SASLC__CONFIG_SUFFIX);
		if (saslc__parse_file(ctx, path, mech_node->prop) == -1)
			return -1;
	}

	return 0;
}
