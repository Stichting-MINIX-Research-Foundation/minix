/*	$NetBSD: _env.c,v 1.6 2011/10/06 20:31:41 christos Exp $ */

/*-
 * Copyright (c) 2010 The NetBSD Foundation, Inc.
 * All rights reserved.
 *
 * This code is derived from software contributed to The NetBSD Foundation
 * by Matthias Scheler.
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

#include <sys/cdefs.h>
#if defined(LIBC_SCCS) && !defined(lint)
__RCSID("$NetBSD: _env.c,v 1.6 2011/10/06 20:31:41 christos Exp $");
#endif /* LIBC_SCCS and not lint */

#include "namespace.h"

#include <sys/rbtree.h>

#include <assert.h>
#include <errno.h>
#include <limits.h>
#include <stdlib.h>
#include <stddef.h>
#include <string.h>

#include "env.h"
#include "reentrant.h"
#include "local.h"

/*
 * Red-Black tree node for tracking memory used by environment variables.
 * The tree is sorted by the address of the nodes themselves.
 */
typedef struct {
	rb_node_t	rb_node;
	size_t		length;
	uint8_t		marker;
	char		data[];
} env_node_t;

/* Compare functions for above tree. */
static signed int env_tree_compare_nodes(void *, const void *, const void *);
static signed int env_tree_compare_key(void *, const void *, const void *);

/* Operations for above tree. */
static const rb_tree_ops_t env_tree_ops = {
	.rbto_compare_nodes = env_tree_compare_nodes,
	.rbto_compare_key = env_tree_compare_key,
	.rbto_node_offset = offsetof(env_node_t, rb_node),
	.rbto_context = NULL
};

/* The single instance of above tree. */
static rb_tree_t	env_tree;

/* The allocated environment. */
static char	**allocated_environ;
static size_t	allocated_environ_size;

#define	ENV_ARRAY_SIZE_MIN	16

/* The lock protecting access to the environment. */
#ifdef _REENTRANT
static rwlock_t env_lock = RWLOCK_INITIALIZER;
#endif

/* Compatibility function. */
char *__findenv(const char *name, int *offsetp);

__warn_references(__findenv,
    "warning: __findenv is an internal obsolete function.")

/* Our initialization function. */
void __libc_env_init(void);

char **environ;

/*ARGSUSED*/
static signed int
env_tree_compare_nodes(void *ctx, const void *node_a, const void *node_b)
{
	uintptr_t addr_a, addr_b;

	addr_a = (uintptr_t)node_a;
	addr_b = (uintptr_t)node_b;

	if (addr_a < addr_b)
		return -1;

	if (addr_a > addr_b)
		return 1;

	return 0;
}

static signed int
env_tree_compare_key(void *ctx, const void *node, const void *key)
{
	return env_tree_compare_nodes(ctx, node,
	    (const uint8_t *)key - offsetof(env_node_t, data));
}

/*
 * Determine the of the name in an environment string. Return 0 if the
 * name is not valid.
 */
size_t
__envvarnamelen(const char *str, bool withequal)
{
	size_t l_name;

	if (str == NULL)
		return 0;

	l_name = strcspn(str, "=");
	if (l_name == 0)
		return 0;

	if (withequal) {
		if (str[l_name] != '=')
			return 0;
	} else {
		if (str[l_name] == '=')
			return 0;
	}

	return l_name;
}

/*
 * Free memory occupied by environment variable if possible. This function
 * must be called with the environment write locked.
 */
void
__freeenvvar(char *envvar)
{
	env_node_t *node;

	_DIAGASSERT(envvar != NULL);
	node = rb_tree_find_node(&env_tree, envvar);
	if (node != NULL) {
		rb_tree_remove_node(&env_tree, node);
		free(node);
	}
}

/*
 * Allocate memory for an environment variable. This function must be called
 * with the environment write locked.
 */
char *
__allocenvvar(size_t length)
{
	env_node_t *node;

	node = malloc(sizeof(*node) + length);
	if (node != NULL) {
		node->length = length;
		node->marker = 0;
		rb_tree_insert_node(&env_tree, node);
		return node->data;
	} else {
		return NULL;
	}
}

/*
 * Check whether an environment variable is writable. This function must be
 * called with the environment write locked as the caller will probably
 * overwrite the environment variable afterwards.
 */
bool
__canoverwriteenvvar(char *envvar, size_t length)
{
	env_node_t *node;

	_DIAGASSERT(envvar != NULL);

	node = rb_tree_find_node(&env_tree, envvar);
	return (node != NULL && length <= node->length);
}

/* Free all allocated environment variables that are no longer used. */
static void
__scrubenv(void)
{
	static uint8_t marker = 0;
	size_t num_entries;
	env_node_t *node, *next;

	while (++marker == 0);

	/* Mark all nodes which are currently used. */
	for (num_entries = 0; environ[num_entries] != NULL; num_entries++) {
		node = rb_tree_find_node(&env_tree, environ[num_entries]);
		if (node != NULL)
			node->marker = marker;
	}

	/* Free all nodes which are currently not used. */
	for (node = RB_TREE_MIN(&env_tree); node != NULL; node = next) {
		next = rb_tree_iterate(&env_tree, node, RB_DIR_RIGHT);

		if (node->marker != marker) {
			rb_tree_remove_node(&env_tree, node);
			free(node);
		}
	}

	/* Deal with the environment array itself. */
	if (environ == allocated_environ) {
		/* Clear out spurious entries in the environment. */
		(void)memset(&environ[num_entries + 1], 0,
		    (allocated_environ_size - num_entries - 1) *
		    sizeof(*environ));
	} else {
		/*
		 * The environment array was not allocated by "libc".
		 * Free our array if we allocated one.
		 */
		free(allocated_environ);
		allocated_environ = NULL;
		allocated_environ_size = 0;
	}
}

/*
 * Get a (new) slot in the environment. This function must be called with
 * the environment write locked.
 */
ssize_t
__getenvslot(const char *name, size_t l_name, bool allocate)
{
	size_t new_size, num_entries, required_size;
	char **new_environ;

	/* Does the environ need scrubbing? */
	if (environ != allocated_environ && allocated_environ != NULL)
		__scrubenv();

	/* Search for an existing environment variable of the given name. */
	num_entries = 0;
	while (environ[num_entries] != NULL) {
		if (strncmp(environ[num_entries], name, l_name) == 0 &&
		    environ[num_entries][l_name] == '=') {
			/* We found a match. */
			return num_entries;
		}
		num_entries ++;
	}

	/* No match found, return if we don't want to allocate a new slot. */
	if (!allocate)
		return -1;

	/* Create a new slot in the environment. */
	required_size = num_entries + 1;
	if (environ == allocated_environ &&
	    required_size < allocated_environ_size) {
		/* Does the environment need scrubbing? */
		if (required_size < allocated_environ_size &&
		    allocated_environ[required_size] != NULL) {
			__scrubenv();
		}

		/* Return a free slot. */
		return num_entries;
	}

	/* Determine size of a new environment array. */
	new_size = ENV_ARRAY_SIZE_MIN;
	while (new_size <= required_size)
		new_size <<= 1;

	/* Allocate a new environment array. */
	if (environ == allocated_environ) {
		new_environ = realloc(environ,
		    new_size * sizeof(*new_environ));
		if (new_environ == NULL)
			return -1;
	} else {
		free(allocated_environ);
		allocated_environ = NULL;
		allocated_environ_size = 0;

		new_environ = malloc(new_size * sizeof(*new_environ));
		if (new_environ == NULL)
			return -1;
		(void)memcpy(new_environ, environ,
		    num_entries * sizeof(*new_environ));
	}

	/* Clear remaining entries. */
	(void)memset(&new_environ[num_entries], 0,
	    (new_size - num_entries) * sizeof(*new_environ));

	/* Use the new environment array. */
	environ = allocated_environ = new_environ;
	allocated_environ_size = new_size;

	/* Return a free slot. */
	return num_entries;
}

/* Find a string in the environment. */
char *
__findenvvar(const char *name, size_t l_name)
{
	ssize_t offset;

	offset = __getenvslot(name, l_name, false);
	return (offset != -1) ? environ[offset] + l_name + 1 : NULL;
}

/* Compatibility interface, do *not* call this function. */
char *
__findenv(const char *name, int *offsetp)
{
	size_t l_name;
	ssize_t offset;

	l_name = __envvarnamelen(name, false);
	if (l_name == 0)
		return NULL;

	offset = __getenvslot(name, l_name, false);
	if (offset < 0 || offset > INT_MAX)
		return NULL;

	*offsetp = (int)offset;
	return environ[offset] + l_name + 1;
}

#ifdef _REENTRANT

/* Lock the environment for read. */
bool
__readlockenv(void)
{
	int error;

	error = rwlock_rdlock(&env_lock);
	if (error == 0)
		return true;

	errno = error;
	return false;
}

/* Lock the environment for write. */
bool
__writelockenv(void)
{
	int error;

	error = rwlock_wrlock(&env_lock);
	if (error == 0)
		return true;

	errno = error;
	return false;
}

/* Unlock the environment for write. */
bool
__unlockenv(void)
{
	int error;

	error = rwlock_unlock(&env_lock);
	if (error == 0)
		return true;

	errno = error;
	return false;
}

#endif

/* Initialize environment memory RB tree. */
void
__libc_env_init(void)
{
	rb_tree_init(&env_tree, &env_tree_ops);
}
