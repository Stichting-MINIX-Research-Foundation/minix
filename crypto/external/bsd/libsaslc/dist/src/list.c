/* $NetBSD: list.c,v 1.3 2011/02/20 01:59:46 christos Exp $ */

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
__RCSID("$NetBSD: list.c,v 1.3 2011/02/20 01:59:46 christos Exp $");

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "list.h"
#include "msg.h"

/**
 * @brief test for linear white space
 * @param ch character to test
 * @return true if LWS, false otherwise.
 */
static int
is_LWS(const char ch)
{

	return ch == '\n' || ch == ' ' || ch == '\t';
}

/**
 * @brief skip leading whitespace (does not modify string)
 * @param p start of string
 * @return pointer to string after leading whitespace
 */
static const char *
skip_LWS(const char *p)
{

	while (*p != '\0' && is_LWS((unsigned char)*p))
		p++;
	return p;
}

/**
 * @brief remove trailing whitespace from a string (does not modify
 * string)
 * @param p pointer to start of trailing whitespace
 * @param s pointer to start of string
 * @return pointer to new end of string
 */
static const char *
strip_LWS(const char *p, const char *s)
{

	if (p <= s)
		return s;
	while (p >= s && is_LWS((unsigned char)*p))
		p--;
	return p + 1;
}

/**
 * @brief find the next element in a comma delimited list
 * @param p pointer to list string
 * @return pointer to start of next element in list
 */
static const char *
next_element(const char *p)
{
	int quoting;
	int escaped;

	quoting = 0;
	escaped = 0;
	for (; *p != '\0'; p++) {
		switch (*p) {
		case ',':
			if (!quoting && !escaped)
				return p + 1;
			escaped = 0;
			break;
		case '"':
			if (!escaped)
				quoting = !quoting;
			escaped = 0;
			break;
		case '\\':
			escaped = !escaped;
			break;
		default:
			escaped = 0;
			break;
		}
	}
	return p;
}

/**
 * @brief allocate a list_t node, the memory for its value, and save
 * the value.
 * @param b buffer containing data to save in node value
 * @param len length of data to save
 * @return the list_t node, or NULL on failure.
 */
static list_t *
alloc_list(const char *b, size_t len)
{
	list_t *l;

	if ((l = malloc(sizeof(*l))) == NULL)
		return NULL;
	if ((l->value = malloc(len + 1)) == NULL) {
		free(l);
		return NULL;
	}
	memcpy(l->value, b, len);
	l->value[len]  = '\0';
	l->next = NULL;
	return l;
}

/**
 * @brief free an allocated list
 * @param l the list
 */
void
saslc__list_free(list_t *l)
{
	list_t *n;

	for (/*EMPTY*/; l != NULL; l = n) {
		n = l->next;
		free(l->value);
		free(l);
	}
}

/**
 * @brief Parse a list of the following format:
 *   ( *LWS element *( *LWS "," *LWS element ))
 * @param lp pointer to list_t type for returned list.  Cannot be NULL.
 * @param p string to parse
 * @return 0 on success, -1 on error (no memory).
 *
 * Note: the list is allocated.  Use saslc__list_free() to free it.
 */
int
saslc__list_parse(list_t **lp, const char *p)
{
	const char *e, *n;
	list_t *l, *t, **tp;

	l = NULL;
	tp = NULL;
	n = p;
	for (;;) {
		p = n;
		p = skip_LWS(p);
		if (*p == '\0')
			break;
		n = next_element(p);
		e = n > p && n[-1] == ',' ? n - 1 : n;
		e = strip_LWS(e - 1, p);
		if (e <= p)
			continue;
		t = alloc_list(p, (size_t)(e - p));
		if (t == NULL) {
			saslc__list_free(l);
			return -1;
		}
		if (tp != NULL)
			*tp = t;
		else
			l = t;
		tp = &t->next;
	}
	*lp = l;
	return 0;
}

/**
 * @brief allocate a new list node for a string and append it to a
 * list
 * @param l the list to append
 * @param p the string
 */
int
saslc__list_append(list_t **l, const char *p)
{
	list_t *n, *e;

	e = NULL;
	for (n = *l; n != NULL; n = n->next)
		e = n;

	n = alloc_list(p, strlen(p));
	if (n == NULL)
		return -1;

	if (e == NULL)
		*l = n;
	else
		e->next = n;

	return 0;
}

/**
 * @brief get the flags corresponding to a given name.
 * @param key the key to find the flags for.
 * @param tbl the NULL terminated named_flag_t table to use for the
 * lookup.
 * @return the flags if found or zero if not.
 */
static unsigned int
get_named_flag(const char *key, const named_flag_t *tbl)
{
	const named_flag_t *p;

	for (p = tbl; p->name != NULL; p++) {
		if (strcasecmp(key, p->name) == 0)
			return p->flag;
	}
	return 0;
}

/**
 * @brief collect all the flags from a list of flag names.
 * @param list the list
 * @param tbl the NULL terminated named_flag_t table to use for the
 * lookups
 * @return the or-ed flags of all the matches
 */
uint32_t
saslc__list_flags(list_t *list, const named_flag_t *tbl)
{
	list_t *l;
	uint32_t flags;

	flags = 0;
	for (l = list; l != NULL; l = l->next)
		flags |= get_named_flag(l->value, tbl);

	return flags;
}

/**
 * @brief print all the values in a list if debugging is enabled
 * @param list the list
 * @param str a string to print before the results.
 *
 * XXX: move this to msg.c?
 */
void
saslc__list_log(list_t *list, const char *str)
{
	list_t *l;

	if (!saslc_debug)
		return;

	saslc__msg_dbg("%s", str);
	for (l = list; l != NULL; l = l->next)
		saslc__msg_dbg("  value: '%s'\n",
		    l->value ? l->value : "<null>");
}
