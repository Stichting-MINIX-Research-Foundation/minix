/*	$NetBSD: graph2.c,v 1.4 2011/10/21 23:47:11 joerg Exp $	*/
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
__RCSID("$NetBSD: graph2.c,v 1.4 2011/10/21 23:47:11 joerg Exp $");

#include <err.h>
#include <inttypes.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "nbperf.h"
#include "graph2.h"

static const uint32_t unused = 0xffffffffU;

void
graph2_setup(struct graph2 *graph, uint32_t v, uint32_t e)
{
	graph->v = v;
	graph->e = e;

	graph->verts = calloc(sizeof(struct vertex2), v);
	graph->edges = calloc(sizeof(struct edge2), e);
	graph->output_order = calloc(sizeof(uint32_t), e);

	if (graph->verts == NULL || graph->edges == NULL ||
	    graph->output_order == NULL)
		err(1, "malloc failed");
}

void
graph2_free(struct graph2 *graph)
{
	free(graph->verts);
	free(graph->edges);
	free(graph->output_order);

	graph->verts = NULL;
	graph->edges = NULL;
	graph->output_order = NULL;
}

static int
graph2_check_duplicates(struct nbperf *nbperf, struct graph2 *graph)
{
	struct vertex2 *v;
	struct edge2 *e, *e2;
	uint32_t i, j;

	for (i = 0; i < graph->e; ++i) {
		e = &graph->edges[i];
		v = &graph->verts[e->left];
		j = v->l_edge;
		e2 = &graph->edges[j];
		for (;;) {
			if (i < j && e->right == e2->right &&
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
graph2_hash(struct nbperf *nbperf, struct graph2 *graph)
{
	struct vertex2 *v;
	uint32_t hashes[NBPERF_MAX_HASH_SIZE];
	size_t i;

	for (i = 0; i < graph->e; ++i) {
		(*nbperf->compute_hash)(nbperf,
		    nbperf->keys[i], nbperf->keylens[i], hashes);
		graph->edges[i].left = hashes[0] % graph->v;
		graph->edges[i].right = hashes[1] % graph->v;
		if (graph->edges[i].left == graph->edges[i].right)
			return -1;
	}

	for (i = 0; i < graph->v; ++i) {
		graph->verts[i].l_edge = unused;
		graph->verts[i].r_edge = unused;
	}

	for (i = 0; i < graph->e; ++i) {
		v = &graph->verts[graph->edges[i].left];
		if (v->l_edge != unused)
			graph->edges[v->l_edge].l_prev = i;
		graph->edges[i].l_next = v->l_edge;
		graph->edges[i].l_prev = unused;
		v->l_edge = i;

		v = &graph->verts[graph->edges[i].right];
		if (v->r_edge != unused)
			graph->edges[v->r_edge].r_prev = i;
		graph->edges[i].r_next = v->r_edge;
		graph->edges[i].r_prev = unused;
		v->r_edge = i;
	}

	if (nbperf->first_round) {
		nbperf->first_round = 0;
		return graph2_check_duplicates(nbperf, graph);
	}

	return 0;
}

static void
graph2_remove_vertex(struct graph2 *graph, struct vertex2 *v)
{
	struct edge2 *e;
	struct vertex2 *v2;

	for (;;) {
		if (v->l_edge != unused && v->r_edge != unused)
			break;
		if (v->l_edge == unused && v->r_edge == unused)
			break;

		if (v->l_edge != unused) {
			e = &graph->edges[v->l_edge];
			if (e->l_next != unused)
				break;
			v->l_edge = unused; /* No other elements possible! */
			v2 = &graph->verts[e->right];
			if (e->r_prev == unused)
				v2->r_edge = e->r_next;
			else
				graph->edges[e->r_prev].r_next = e->r_next;
			if (e->r_next != unused)
				graph->edges[e->r_next].r_prev = e->r_prev;
			v = v2;
		} else {
			e = &graph->edges[v->r_edge];
			if (e->r_next != unused)
				break;
			v->r_edge = unused; /* No other elements possible! */
			v2 = &graph->verts[e->left];
			if (e->l_prev == unused)
				v2->l_edge = e->l_next;
			else
				graph->edges[e->l_prev].l_next = e->l_next;
			if (e->l_next != unused)
				graph->edges[e->l_next].l_prev = e->l_prev;
			v = v2;
		}

		graph->output_order[--graph->output_index] = e - graph->edges;
	}
}

int
graph2_output_order(struct graph2 *graph)
{
	size_t i;

	graph->output_index = graph->e;

	for (i = 0; i < graph->v; ++i)
		graph2_remove_vertex(graph, &graph->verts[i]);

	if (graph->output_index != 0)
		return -1;

	return 0;
}
