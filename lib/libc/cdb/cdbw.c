/*	$NetBSD: cdbw.c,v 1.5 2012/07/21 22:49:37 joerg Exp $	*/
/*-
 * Copyright (c) 2009, 2010 The NetBSD Foundation, Inc.
 * All rights reserved.
 *
 * This code is derived from software contributed to The NetBSD Foundation
 * by Joerg Sonnenberger.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 *
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in
 *    the documentation and/or other materials provided with the
 *    distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * ``AS IS'' AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS
 * FOR A PARTICULAR PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL THE
 * COPYRIGHT HOLDERS OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT,
 * INCIDENTAL, SPECIAL, EXEMPLARY OR CONSEQUENTIAL DAMAGES (INCLUDING,
 * BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
 * LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED
 * AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY,
 * OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT
 * OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 */

#if HAVE_NBTOOL_CONFIG_H
#include "nbtool_config.h"
#endif

#include <sys/cdefs.h>
__RCSID("$NetBSD: cdbw.c,v 1.5 2012/07/21 22:49:37 joerg Exp $");

#include "namespace.h"

#if !HAVE_NBTOOL_CONFIG_H || HAVE_SYS_ENDIAN_H
#include <sys/endian.h>
#endif
#include <sys/queue.h>
#include <cdbw.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#ifdef __weak_alias
__weak_alias(cdbw_close,_cdbw_close)
__weak_alias(cdbw_open,_cdbw_open)
__weak_alias(cdbw_output,_cdbw_output)
__weak_alias(cdbw_put,_cdbw_put)
__weak_alias(cdbw_put_data,_cdbw_put_data)
__weak_alias(cdbw_put_key,_cdbw_put_key)
#endif

struct key_hash {
	SLIST_ENTRY(key_hash) link;
	uint32_t hashes[3];
	uint32_t idx;
	void *key;
	size_t keylen;
};

SLIST_HEAD(key_hash_head, key_hash);

struct cdbw {
	size_t data_counter;
	size_t data_allocated;
	size_t data_size;
	size_t *data_len;
	void **data_ptr;

	size_t hash_size;
	struct key_hash_head *hash;
	size_t key_counter;
};

 /* Max. data counter that allows the index size to be 32bit. */
static const uint32_t max_data_counter = 0xccccccccU;

struct cdbw *
cdbw_open(void)
{
	struct cdbw *cdbw;
	size_t i;

	cdbw = calloc(sizeof(*cdbw), 1);
	if (cdbw == NULL)
		return NULL;

	cdbw->hash_size = 1024;
	cdbw->hash = calloc(cdbw->hash_size, sizeof(*cdbw->hash));
	if (cdbw->hash == NULL) {
		free(cdbw);
		return NULL;
	}

	for (i = 0; i < cdbw->hash_size; ++i)
		SLIST_INIT(cdbw->hash + i);

	return cdbw;
}

int
cdbw_put(struct cdbw *cdbw, const void *key, size_t keylen,
    const void *data, size_t datalen)
{
	uint32_t idx;
	int rv;

	rv = cdbw_put_data(cdbw, data, datalen, &idx);
	if (rv)
		return rv;
	rv = cdbw_put_key(cdbw, key, keylen, idx);
	if (rv) {
		--cdbw->data_counter;
		free(cdbw->data_ptr[cdbw->data_counter]);
		cdbw->data_size -= datalen;
		return rv;
	}
	return 0;
}

int
cdbw_put_data(struct cdbw *cdbw, const void *data, size_t datalen,
    uint32_t *idx)
{

	if (cdbw->data_counter == max_data_counter)
		return -1;

	if (cdbw->data_size + datalen < cdbw->data_size ||
	    cdbw->data_size + datalen > 0xffffffffU)
		return -1; /* Overflow */

	if (cdbw->data_allocated == cdbw->data_counter) {
		void **new_data_ptr;
		size_t *new_data_len;
		size_t new_allocated;

		if (cdbw->data_allocated == 0)
			new_allocated = 256;
		else
			new_allocated = cdbw->data_allocated * 2;

		new_data_ptr = realloc(cdbw->data_ptr,
		    sizeof(*cdbw->data_ptr) * new_allocated);
		if (new_data_ptr == NULL)
			return -1;
		cdbw->data_ptr = new_data_ptr;

		new_data_len = realloc(cdbw->data_len,
		    sizeof(*cdbw->data_len) * new_allocated);
		if (new_data_len == NULL)
			return -1;
		cdbw->data_len = new_data_len;

		cdbw->data_allocated = new_allocated;
	}

	cdbw->data_ptr[cdbw->data_counter] = malloc(datalen);
	if (cdbw->data_ptr[cdbw->data_counter] == NULL)
		return -1;
	memcpy(cdbw->data_ptr[cdbw->data_counter], data, datalen);
	cdbw->data_len[cdbw->data_counter] = datalen;
	cdbw->data_size += datalen;
	*idx = cdbw->data_counter++;
	return 0;
}

int
cdbw_put_key(struct cdbw *cdbw, const void *key, size_t keylen, uint32_t idx)
{
	uint32_t hashes[3];
	struct key_hash_head *head, *head2, *new_head;
	struct key_hash *key_hash;
	size_t new_hash_size, i;

	if (idx >= cdbw->data_counter ||
	    cdbw->key_counter == max_data_counter)
		return -1;

	mi_vector_hash(key, keylen, 0, hashes);

	head = cdbw->hash + (hashes[0] & (cdbw->hash_size - 1));
	SLIST_FOREACH(key_hash, head, link) {
		if (key_hash->keylen != keylen)
			continue;
		if (key_hash->hashes[0] != hashes[0])
			continue;
		if (key_hash->hashes[1] != hashes[1])
			continue;
		if (key_hash->hashes[2] != hashes[2])
			continue;
		if (memcmp(key, key_hash->key, keylen))
			continue;
		return -1;
	}
	key_hash = malloc(sizeof(*key_hash));
	if (key_hash == NULL)
		return -1;
	key_hash->key = malloc(keylen);
	if (key_hash->key == NULL) {
		free(key_hash);
		return -1;
	}
	memcpy(key_hash->key, key, keylen);
	key_hash->hashes[0] = hashes[0];
	key_hash->hashes[1] = hashes[1];
	key_hash->hashes[2] = hashes[2];
	key_hash->keylen = keylen;
	key_hash->idx = idx;
	SLIST_INSERT_HEAD(head, key_hash, link);
	++cdbw->key_counter;

	if (cdbw->key_counter <= cdbw->hash_size)
		return 0;

	/* Try to resize the hash table, but ignore errors. */
	new_hash_size = cdbw->hash_size * 2;
	new_head = calloc(sizeof(*new_head), new_hash_size);
	if (new_head == NULL)
		return 0;

	head = &cdbw->hash[hashes[0] & (cdbw->hash_size - 1)];
	for (i = 0; i < new_hash_size; ++i)
		SLIST_INIT(new_head + i);

	for (i = 0; i < cdbw->hash_size; ++i) {
		head = cdbw->hash + i;

		while ((key_hash = SLIST_FIRST(head)) != NULL) {
			SLIST_REMOVE_HEAD(head, link);
			head2 = new_head +
			    (key_hash->hashes[0] & (new_hash_size - 1));
			SLIST_INSERT_HEAD(head2, key_hash, link);
		}
	}
	free(cdbw->hash);
	cdbw->hash_size = new_hash_size;
	cdbw->hash = new_head;

	return 0;
}

void
cdbw_close(struct cdbw *cdbw)
{
	struct key_hash_head *head;
	struct key_hash *key_hash;
	size_t i;

	for (i = 0; i < cdbw->hash_size; ++i) {
		head = cdbw->hash + i;
		while ((key_hash = SLIST_FIRST(head)) != NULL) {
			SLIST_REMOVE_HEAD(head, link);
			free(key_hash->key);
			free(key_hash);
		}
	}

	for (i = 0; i < cdbw->data_counter; ++i)
		free(cdbw->data_ptr[i]);
	free(cdbw->data_ptr);
	free(cdbw->data_len);
	free(cdbw->hash);
	free(cdbw);
}

uint32_t
cdbw_stable_seeder(void)
{
	return 0;
}

#define unused 0xffffffffU

struct vertex {
	uint32_t l_edge, m_edge, r_edge;
};

struct edge {
	uint32_t idx;

	uint32_t left, middle, right;
	uint32_t l_prev, m_prev, l_next;
	uint32_t r_prev, m_next, r_next;
};

struct state {
	uint32_t data_entries;
	uint32_t entries;
	uint32_t keys;
	uint32_t seed;

	uint32_t *g;
	char *visited;

	struct vertex *verts;
	struct edge *edges;
	uint32_t output_index;
	uint32_t *output_order;
};

static void
remove_vertex(struct state *state, struct vertex *v)
{
	struct edge *e;
	struct vertex *vl, *vm, *vr;

	if (v->l_edge != unused && v->m_edge != unused)
		return;
	if (v->l_edge != unused && v->r_edge != unused)
		return;
	if (v->m_edge != unused && v->r_edge != unused)
		return;
	if (v->l_edge == unused && v->m_edge == unused && v->r_edge == unused)
		return;

	if (v->l_edge != unused) {
		e = &state->edges[v->l_edge];
		if (e->l_next != unused)
			return;
	} else if (v->m_edge != unused) {
		e = &state->edges[v->m_edge];
		if (e->m_next != unused)
			return;
	} else {
		if (v->r_edge == unused)
			abort();
		e = &state->edges[v->r_edge];
		if (e->r_next != unused)
			return;
	}

	state->output_order[--state->output_index] = e - state->edges;

	vl = &state->verts[e->left];
	vm = &state->verts[e->middle];
	vr = &state->verts[e->right];

	if (e->l_prev == unused)
		vl->l_edge = e->l_next;
	else
		state->edges[e->l_prev].l_next = e->l_next;
	if (e->l_next != unused)
		state->edges[e->l_next].l_prev = e->l_prev;

	if (e->m_prev == unused)
		vm->m_edge = e->m_next;
	else
		state->edges[e->m_prev].m_next = e->m_next;
	if (e->m_next != unused)
		state->edges[e->m_next].m_prev = e->m_prev;

	if (e->r_prev == unused)
		vr->r_edge = e->r_next;
	else
		state->edges[e->r_prev].r_next = e->r_next;
	if (e->r_next != unused)
		state->edges[e->r_next].r_prev = e->r_prev;
}

static int
build_graph(struct cdbw *cdbw, struct state *state)
{
	struct key_hash_head *head;
	struct key_hash *key_hash;
	struct vertex *v;
	struct edge *e;
	uint32_t hashes[3];
	size_t i;

	e = state->edges;
	for (i = 0; i < cdbw->hash_size; ++i) {
		head = &cdbw->hash[i];
		SLIST_FOREACH(key_hash, head, link) {
			e->idx = key_hash->idx;
			mi_vector_hash(key_hash->key, key_hash->keylen,
			    state->seed, hashes);
			e->left = hashes[0] % state->entries;
			e->middle = hashes[1] % state->entries;
			e->right = hashes[2] % state->entries;

			if (e->left == e->middle)
				return -1;
			if (e->left == e->right)
				return -1;
			if (e->middle == e->right)
				return -1;

			++e;
		}
	}

	for (i = 0; i < state->entries; ++i) {
		v = state->verts + i;
		v->l_edge = unused;
		v->m_edge = unused;
		v->r_edge = unused;
	}

	for (i = 0; i < state->keys; ++i) {
		e = state->edges + i;
		v = state->verts + e->left;
		if (v->l_edge != unused)
			state->edges[v->l_edge].l_prev = i;
		e->l_next = v->l_edge;
		e->l_prev = unused;
		v->l_edge = i;

		v = &state->verts[e->middle];
		if (v->m_edge != unused)
			state->edges[v->m_edge].m_prev = i;
		e->m_next = v->m_edge;
		e->m_prev = unused;
		v->m_edge = i;

		v = &state->verts[e->right];
		if (v->r_edge != unused)
			state->edges[v->r_edge].r_prev = i;
		e->r_next = v->r_edge;
		e->r_prev = unused;
		v->r_edge = i;
	}

	state->output_index = state->keys;
	for (i = 0; i < state->entries; ++i)
		remove_vertex(state, state->verts + i);

	i = state->keys;
	while (i > 0 && i > state->output_index) {
		--i;
		e = state->edges + state->output_order[i];
		remove_vertex(state, state->verts + e->left);
		remove_vertex(state, state->verts + e->middle);
		remove_vertex(state, state->verts + e->right);
	}

	return state->output_index == 0 ? 0 : -1;
}

static void
assign_nodes(struct state *state)
{
	struct edge *e;
	size_t i;

	for (i = 0; i < state->keys; ++i) {
		e = state->edges + state->output_order[i];

		if (!state->visited[e->left]) {
			state->g[e->left] =
			    (2 * state->data_entries + e->idx
			    - state->g[e->middle] - state->g[e->right])
			    % state->data_entries;
		} else if (!state->visited[e->middle]) {
			state->g[e->middle] =
			    (2 * state->data_entries + e->idx
			    - state->g[e->left] - state->g[e->right])
			    % state->data_entries;
		} else {
			state->g[e->right] =
			    (2 * state->data_entries + e->idx
			    - state->g[e->left] - state->g[e->middle])
			    % state->data_entries;
		}
		state->visited[e->left] = 1;
		state->visited[e->middle] = 1;
		state->visited[e->right] = 1;
	}
}

static size_t
compute_size(uint32_t size)
{
	if (size < 0x100)
		return 1;
	else if (size < 0x10000)
		return 2;
	else
		return 4;
}

#define COND_FLUSH_BUFFER(n) do { 				\
	if (__predict_false(cur_pos + (n) >= sizeof(buf))) {	\
		ret = write(fd, buf, cur_pos);			\
		if (ret == -1 || (size_t)ret != cur_pos)	\
			return -1;				\
		cur_pos = 0;					\
	}							\
} while (/* CONSTCOND */ 0)

static int
print_hash(struct cdbw *cdbw, struct state *state, int fd, const char *descr)
{
	uint32_t data_size;
	uint8_t buf[90000];
	size_t i, size, size2, cur_pos;
	ssize_t ret;

	memcpy(buf, "NBCDB\n\0", 7);
	buf[7] = 1;
	strncpy((char *)buf + 8, descr, 16);
	le32enc(buf + 24, cdbw->data_size);
	le32enc(buf + 28, cdbw->data_counter);
	le32enc(buf + 32, state->entries);
	le32enc(buf + 36, state->seed);
	cur_pos = 40;

	size = compute_size(state->entries);
	for (i = 0; i < state->entries; ++i) {
		COND_FLUSH_BUFFER(4);
		le32enc(buf + cur_pos, state->g[i]);
		cur_pos += size;
	}
	size2 = compute_size(cdbw->data_size);
	size = size * state->entries % size2;
	if (size != 0) {
		size = size2 - size;
		COND_FLUSH_BUFFER(4);
		le32enc(buf + cur_pos, 0);
		cur_pos += size;
	}
	for (data_size = 0, i = 0; i < cdbw->data_counter; ++i) {
		COND_FLUSH_BUFFER(4);
		le32enc(buf + cur_pos, data_size);
		cur_pos += size2;
		data_size += cdbw->data_len[i];
	}
	COND_FLUSH_BUFFER(4);
	le32enc(buf + cur_pos, data_size);
	cur_pos += size2;

	for (i = 0; i < cdbw->data_counter; ++i) {
		COND_FLUSH_BUFFER(cdbw->data_len[i]);
		if (cdbw->data_len[i] < sizeof(buf)) {
			memcpy(buf + cur_pos, cdbw->data_ptr[i],
			    cdbw->data_len[i]);
			cur_pos += cdbw->data_len[i];
		} else {
			ret = write(fd, cdbw->data_ptr[i], cdbw->data_len[i]);
			if (ret == -1 || (size_t)ret != cdbw->data_len[i])
				return -1;
		}
	}
	if (cur_pos != 0) {
		ret = write(fd, buf, cur_pos);
		if (ret == -1 || (size_t)ret != cur_pos)
			return -1;
	}
	return 0;
}

int
cdbw_output(struct cdbw *cdbw, int fd, const char descr[16],
    uint32_t (*seedgen)(void))
{
	struct state state;
	int rv;

	if (cdbw->data_counter == 0 || cdbw->key_counter == 0) {
		state.entries = 0;
		state.seed = 0;
		print_hash(cdbw, &state, fd, descr);
		return 0;
	}

#if HAVE_NBTOOL_CONFIG_H
	if (seedgen == NULL)
		seedgen = cdbw_stable_seeder;
#else
	if (seedgen == NULL)
		seedgen = arc4random;
#endif

	rv = 0;

	state.keys = cdbw->key_counter;
	state.data_entries = cdbw->data_counter;
	state.entries = state.keys + (state.keys + 3) / 4;
	if (state.entries < 10)
		state.entries = 10;

#define	NALLOC(var, n)	var = calloc(sizeof(*var), n)
	NALLOC(state.g, state.entries);
	NALLOC(state.visited, state.entries);
	NALLOC(state.verts, state.entries);
	NALLOC(state.edges, state.entries);
	NALLOC(state.output_order, state.keys);
#undef NALLOC

	if (state.g == NULL || state.visited == NULL || state.verts == NULL ||
	    state.edges == NULL || state.output_order == NULL) {
		rv = -1;
		goto release;
	}

	state.seed = 0;
	do {
		if (seedgen == cdbw_stable_seeder)
			++state.seed;
		else
			state.seed = (*seedgen)();
	} while (build_graph(cdbw, &state));

	assign_nodes(&state);
	rv = print_hash(cdbw, &state, fd, descr);

release:
	free(state.g);
	free(state.visited);
	free(state.verts);
	free(state.edges);
	free(state.output_order);

	return rv;
}
