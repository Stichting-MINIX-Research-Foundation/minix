/*	$NetBSD: graph3.c,v 1.4 2011/10/21 23:47:11 joerg Exp $	*/
/*-
 * Copyright (c) 2009 The NetBSD Foundation, Inc.
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
__RCSID("$NetBSD: graph3.c,v 1.4 2011/10/21 23:47:11 joerg Exp $");

#include <err.h>
#include <inttypes.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "nbperf.h"
#include "graph3.h"

static const uint32_t unused = 0xffffffffU;

void
graph3_setup(struct graph3 *graph, uint32_t v, uint32_t e)
{
	graph->v = v;
	graph->e = e;

	graph->verts = calloc(sizeof(struct vertex3), v);
	graph->edges = calloc(sizeof(struct edge3), e);
	graph->output_order = calloc(sizeof(uint32_t), e);

	if (graph->verts == NULL || graph->edges == NULL ||
	    graph->output_order == NULL)
		err(1, "malloc failed");
}

void
graph3_free(struct graph3 *graph)
{
	free(graph->verts);
	free(graph->edges);
	free(graph->output_order);

	graph->verts = NULL;
	graph->edges = NULL;
	graph->output_order = NULL;
}

static int
graph3_check_duplicates(struct nbperf *nbperf, struct graph3 *graph)
{
	struct vertex3 *v;
	struct edge3 *e, *e2;
	uint32_t i, j;

	for (i = 0; i < graph->e; ++i) {
		e = &graph->edges[i];
		v = &graph->verts[e->left];
		j = v->l_edge;
		e2 = &graph->edges[j];
		for (;;) {
			if (i < j && e->middle == e2->middle &&
			    e->right == e2->right &&
			    nbperf->keylens[i] == nbperf->keylens[j] &&
			    memcmp(nbperf->keys[i], nbperf->keys[j],
			    nbperf->keylens[i]) == 0) {
				nbperf->has_duplicates = 1;
				return -1;
			}
			if (e2->l_next == unused)
				break;
			j = e2->l_next;
			e2 = &graph->edges[j];
		}		
	}
	return 0;
}

int
graph3_hash(struct nbperf *nbperf, struct graph3 *graph)
{
	struct vertex3 *v;
	uint32_t hashes[NBPERF_MAX_HASH_SIZE];
	size_t i;

	for (i = 0; i < graph->e; ++i) {
		(*nbperf->compute_hash)(nbperf,
		    nbperf->keys[i], nbperf->keylens[i], hashes);
		graph->edges[i].left = hashes[0] % graph->v;
		graph->edges[i].middle = hashes[1] % graph->v;
		graph->edges[i].right = hashes[2] % graph->v;
		if (graph->edges[i].left == graph->edges[i].middle)
			return -1;
		if (graph->edges[i].left == graph->edges[i].right)
			return -1;
		if (graph->edges[i].middle == graph->edges[i].right)
			return -1;
	}

	for (i = 0; i < graph->v; ++i) {
		graph->verts[i].l_edge = unused;
		graph->verts[i].m_edge = unused;
		graph->verts[i].r_edge = unused;
	}

	for (i = 0; i < graph->e; ++i) {
		v = &graph->verts[graph->edges[i].left];
		if (v->l_edge != unused)
			graph->edges[v->l_edge].l_prev = i;
		graph->edges[i].l_next = v->l_edge;
		graph->edges[i].l_prev = unused;
		v->l_edge = i;

		v = &graph->verts[graph->edges[i].middle];
		if (v->m_edge != unused)
			graph->edges[v->m_edge].m_prev = i;
		graph->edges[i].m_next = v->m_edge;
		graph->edges[i].m_prev = unused;
		v->m_edge = i;

		v = &graph->verts[graph->edges[i].right];
		if (v->r_edge != unused)
			graph->edges[v->r_edge].r_prev = i;
		graph->edges[i].r_next = v->r_edge;
		graph->edges[i].r_prev = unused;
		v->r_edge = i;
	}

	if (nbperf->first_round) {
		nbperf->first_round = 0;
		return graph3_check_duplicates(nbperf, graph);
	}

	return 0;
}

static void
graph3_remove_vertex(struct graph3 *graph, struct vertex3 *v)
{
	struct edge3 *e;
	struct vertex3 *vl, *vm, *vr;

	if (v->l_edge != unused && v->m_edge != unused)
		return;
	if (v->l_edge != unused && v->r_edge != unused)
		return;
	if (v->m_edge != unused && v->r_edge != unused)
		return;
	if (v->l_edge == unused && v->m_edge == unused && v->r_edge == unused)
		return;

	if (v->l_edge != unused) {
		e = &graph->edges[v->l_edge];
		if (e->l_next != unused)
			return;
	} else if (v->m_edge != unused) {
		e = &graph->edges[v->m_edge];
		if (e->m_next != unused)
			return;
	} else {
		if (v->r_edge == unused)
			abort();
		e = &graph->edges[v->r_edge];
		if (e->r_next != unused)
			return;
	}

	graph->output_order[--graph->output_index] = e - graph->edges;

	vl = &graph->verts[e->left];
	vm = &graph->verts[e->middle];
	vr = &graph->verts[e->right];

	if (e->l_prev == unused)
		vl->l_edge = e->l_next;
	else
		graph->edges[e->l_prev].l_next = e->l_next;
	if (e->l_next != unused)
		graph->edges[e->l_next].l_prev = e->l_prev;

	if (e->m_prev == unused)
		vm->m_edge = e->m_next;
	else
		graph->edges[e->m_prev].m_next = e->m_next;
	if (e->m_next != unused)
		graph->edges[e->m_next].m_prev = e->m_prev;

	if (e->r_prev == unused)
		vr->r_edge = e->r_next;
	else
		graph->edges[e->r_prev].r_next = e->r_next;
	if (e->r_next != unused)
		graph->edges[e->r_next].r_prev = e->r_prev;
}

int
graph3_output_order(struct graph3 *graph)
{
	struct edge3 *e;
	size_t i;

	graph->output_index = graph->e;

	for (i = 0; i < graph->v; ++i)
		graph3_remove_vertex(graph, &graph->verts[i]);

	for (i = graph->e; i > 0 && i > graph->output_index;) {
		--i;
		e = &graph->edges[graph->output_order[i]];

		graph3_remove_vertex(graph, &graph->verts[e->left]);
		graph3_remove_vertex(graph, &graph->verts[e->middle]);
		graph3_remove_vertex(graph, &graph->verts[e->right]);
	}

	if (graph->output_index != 0)
		return -1;

	return 0;
}
