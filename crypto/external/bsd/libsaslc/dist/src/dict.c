/* $NetBSD: dict.c,v 1.9 2013/08/28 17:47:07 riastradh Exp $ */

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
__RCSID("$NetBSD: dict.c,v 1.9 2013/08/28 17:47:07 riastradh Exp $");

#include <sys/queue.h>

#include <ctype.h>
#include <errno.h>
#include <stdbool.h>
#include <stdlib.h>
#include <string.h>

#include "dict.h"
#include "msg.h"

/** dictionary */
LIST_HEAD(saslc__dict_t, saslc__dict_node_t);

/** dictionary linked list */
typedef struct saslc__dict_node_t {
	LIST_ENTRY(saslc__dict_node_t) nodes;
	char * key;		/* key */
	char * value;		/* value */
	size_t value_len;	/* value length */
} saslc__dict_node_t;

/*
 * XXX: If you add or change property keys, please readjust these
 * values so that saslc__dict_hashval() remains collisionless.
 * dist/test_hash/test_hash.c can help with this.
 */
/* no collisions: hsize=18  hinit=0  shift=2 */
#define HASH_SIZE       18
#define HASH_INIT       0
#define HASH_SHIFT      2

/**
 * @brief compute the hash value for a given string.
 * @param cp string to hash.
 * @return the hash value.
 *
 * NB: The defines HASH_INIT, HASH_SHIFT, and HASH_SIZE should be
 * adjusted to make this collisionless for the keys used.
 */
static size_t
saslc__dict_hashval(const char *cp)
{
	size_t hval;

	hval = HASH_INIT;
	for (/*EMPTY*/; *cp != '\0'; cp++) {
		hval <<= HASH_SHIFT;
		hval += (size_t)*cp;
	}
	return hval % HASH_SIZE;
}

/**
 * @brief return the hash bucket corresponding to the key string
 * @param dict dictionary to use
 * @param cp key to use for lookup
 * @return the hash bucket for the key.
 */
static saslc__dict_t *
saslc__dict_hash(saslc__dict_t *dict, const char *cp)
{

	return dict + saslc__dict_hashval(cp);
}

/**
 * @brief checks if the key is legal.
 * @param key node key - must not be NULL
 * @return true if key is legal, false otherwise
 *
 * Note: A legal key begins with an isalpha(3) character and is
 * followed by isalnum(3) or '_' characters.
 */
static bool
saslc__dict_valid_key(const char *key)
{

        /* key is not NULL */
	if (!isalpha((unsigned char)*key))
		return false;

	key++;
	while (isalnum((unsigned char)*key) || *key == '_')
		key++;

	return *key == '\0';
}

/**
 * @brief destroys and deallocates list node
 * @param node list node
 */
static void
saslc__dict_list_node_destroy(saslc__dict_node_t *node)
{

	free(node->key);
	/* zero value, it may contain sensitive data */
	explicit_memset(node->value, 0, node->value_len);
	free(node->value);
	LIST_REMOVE(node, nodes);
	free(node);
}

/**
 * @brief gets node from the dictionary using key
 * @param dict dictionary
 * @param key node key
 * @return pointer to node if key is in the dictionary, NULL otherwise
 */
static saslc__dict_node_t *
saslc__dict_get_node_by_key(saslc__dict_t *dict, const char *key)
{
	saslc__dict_node_t *node;

	dict = saslc__dict_hash(dict, key);
	LIST_FOREACH(node, dict, nodes) {
		if (strcmp(node->key, key) == 0)
			return node;
	}
	return NULL;
}

/**
 * @brief destroys and deallocates dictionary
 * @param dict dictionary
 */
void
saslc__dict_destroy(saslc__dict_t *dict)
{
	size_t i;

	for (i = 0; i < HASH_SIZE; i++) {
		while (!LIST_EMPTY(dict + i))
			saslc__dict_list_node_destroy(LIST_FIRST(dict + i));
	}
	free(dict);
}

/**
 * @brief removes node from the dictionary using key
 * @param dict dictionary
 * @param key node key
 * @return DICT_OK on success, DICT_KEYNOTFOUND if node was not found (key
 * does not exist in the dictionary.
 */
saslc__dict_result_t
saslc__dict_remove(saslc__dict_t *dict, const char *key)
{
	saslc__dict_node_t *node;

	node = saslc__dict_get_node_by_key(dict, key);
	if (node == NULL)
		return DICT_KEYNOTFOUND;

	saslc__dict_list_node_destroy(node);
	saslc__msg_dbg("%s: removed key %s", __func__, key);
	return DICT_OK;
}

/**
 * @brief gets node value from the dictionary using key
 * @param dict dictionary
 * @param key node key
 * @return pointer to the value if key was found in the dictionary, NULL
 * otherwise.
 */
const char *
saslc__dict_get(saslc__dict_t *dict, const char *key)
{
	saslc__dict_node_t *node;

	node = saslc__dict_get_node_by_key(dict, key);
	return node != NULL ? node->value : NULL;
}

/**
 * @brief gets length of node value from the dictionary using key
 * @param dict dictionary
 * @param key node key
 * @return length of the node value, 0 is returned in case when key does not
 * exist in the dictionary.
 *
 * XXX: currently unused.
 */
size_t
saslc__dict_get_len(saslc__dict_t *dict, const char *key)
{
	saslc__dict_node_t *node;

	node = saslc__dict_get_node_by_key(dict, key);
	return node != NULL ? node->value_len : 0;
}

/**
 * @brief creates and allocates dictionary
 * @return pointer to new dictionary, NULL is returned on allocation failure
 */
saslc__dict_t *
saslc__dict_create(void)
{
	saslc__dict_t *head;
	int i;

	head = calloc(HASH_SIZE, sizeof(*head));
	if (head == NULL)
		return NULL;

	for (i = 0; i < HASH_SIZE; i++)
		LIST_INIT(head + i);

	return head;
}

/**
 * @brief inserts node into dictionary
 * @param dict dictionary
 * @param key node key
 * @param val node value
 * @return
 * DICT_OK - on success,
 * DICT_KEYINVALID - if node key is illegal,
 * DICT_VALBAD - if node value is illegal,
 * DICT_KEYEXISTS - if node with the same key already exists in the
 * dictionary,
 * DICT_NOMEM - on allocation failure
 */
saslc__dict_result_t
saslc__dict_insert(saslc__dict_t *dict, const char *key, const char *val)
{
	char *d_key, *d_val;
	saslc__dict_node_t *node;

	if (key == NULL || saslc__dict_valid_key(key) == false) {
		saslc__msg_dbg("%s: invalid key: %s", __func__,
		    key ? key : "<null>");
		return DICT_KEYINVALID;
	}
	if (val == NULL) {
		saslc__msg_dbg("%s: NULL value for key %s", __func__, key);
		return DICT_VALBAD;
	}
	/* check if key exists in dictionary */
	if (saslc__dict_get(dict, key) != NULL) {
		saslc__msg_dbg("%s: key exists (ignoring): %s", __func__, key);
		return DICT_KEYEXISTS;
	}
	if ((d_key = strdup(key)) == NULL)
		goto nomem;

	if ((d_val = strdup(val)) == NULL) {
		free(d_key);
		goto nomem;
	}
	if ((node = calloc(1, sizeof(*node))) == NULL) {
		free(d_val);
		free(d_key);
		goto nomem;
	}
	dict = saslc__dict_hash(dict, key);
	if (!LIST_EMPTY(dict))
		saslc__msg_dbg("%s: hash collision: '%s' vs '%s'\n",
		    __func__, key, LIST_FIRST(dict)->key);

	saslc__msg_dbg("%s: %s=\"%s\"", __func__, d_key, d_val);
	LIST_INSERT_HEAD(dict, node, nodes);
	node->key = d_key;
	node->value = d_val;
	node->value_len = strlen(node->value);
	return DICT_OK;
 nomem:
	saslc__msg_dbg("%s: %s", __func__, strerror(errno));
	return DICT_NOMEM;
}
