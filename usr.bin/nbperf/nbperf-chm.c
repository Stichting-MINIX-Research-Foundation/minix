/*	$NetBSD: nbperf-chm.c,v 1.3 2011/10/21 23:47:11 joerg Exp $	*/
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
__RCSID("$NetBSD: nbperf-chm.c,v 1.3 2011/10/21 23:47:11 joerg Exp $");

#include <err.h>
#include <inttypes.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>

#include "nbperf.h"

#ifdef BUILD_CHM3
#include "graph3.h"
#else
#include "graph2.h"
#endif

/*
 * A full description of the algorithm can be found in:
 * "An optimal algorithm for generating minimal perfect hash functions"
 * by Czech, Havas and Majewski in Information Processing Letters,
 * 43(5):256-264, October 1992.
 */

/*
 * The algorithm is based on random, acyclic graphs.
 *
 * Each edge in the represents a key.  The vertices are the reminder of
 * the hash function mod n.  n = cm with c > 2, otherwise the propability
 * of finding an acyclic graph is very low (for 2-graphs).  The constant
 * for 3-graphs is 1.24.
 *
 * After the hashing phase, the graph is checked for cycles.
 * A cycle-free graph is either empty or has a vertex of degree 1.
 * Removing the edge for this vertex doesn't change this property,
 * so applying this recursively reduces the size of the graph.
 * If the graph is empty at the end of the process, it was acyclic.
 *
 * The assignment step now sets g[i] := 0 and processes the edges
 * in reverse order of removal.  That ensures that at least one vertex
 * is always unvisited and can be assigned.
 */

struct state {
#ifdef BUILD_CHM3
	struct graph3 graph;
#else
	struct graph2 graph;
#endif
	uint32_t *g;
	uint8_t *visited;
};

static void
assign_nodes(struct state *state)
{
#ifdef BUILD_CHM3
	struct edge3 *e;
#else
	struct edge2 *e;
#endif
	size_t i;
	uint32_t e_idx;

	for (i = 0; i < state->graph.e; ++i) {
		e_idx = state->graph.output_order[i];
		e = &state->graph.edges[e_idx];

#ifdef BUILD_CHM3
		if (!state->visited[e->left]) {
			state->g[e->left] = (2 * state->graph.e + e_idx
			    - state->g[e->middle] - state->g[e->right])
			    % state->graph.e;
		} else if (!state->visited[e->middle]) {
			state->g[e->middle] = (2 * state->graph.e + e_idx
			    - state->g[e->left] - state->g[e->right])
			    % state->graph.e;
		} else {
			state->g[e->right] = (2 * state->graph.e + e_idx
			    - state->g[e->left] - state->g[e->middle])
			    % state->graph.e;
		}
		state->visited[e->left] = 1;
		state->visited[e->middle] = 1;
		state->visited[e->right] = 1;
#else
		if (!state->visited[e->left]) {
			state->g[e->left] = (state->graph.e + e_idx
			    - state->g[e->right]) % state->graph.e;
		} else {
			state->g[e->right] = (state->graph.e + e_idx
			    - state->g[e->left]) % state->graph.e;
		}
		state->visited[e->left] = 1;
		state->visited[e->right] = 1;
#endif
	}
}

static void
print_hash(struct nbperf *nbperf, struct state *state)
{
	uint32_t i, per_line;
	const char *g_type;
	int g_width;

	fprintf(nbperf->output, "#include <stdlib.h>\n\n");

	fprintf(nbperf->output, "%suint32_t\n",
	    nbperf->static_hash ? "static " : "");
	fprintf(nbperf->output,
	    "%s(const void * __restrict key, size_t keylen)\n",
	    nbperf->hash_name);
	fprintf(nbperf->output, "{\n");
	if (state->graph.v >= 65536) {
		g_type = "uint32_t";
		g_width = 8;
		per_line = 4;
	} else if (state->graph.v >= 256) {
		g_type = "uint16_t";
		g_width = 4;
		per_line = 8;
	} else {
		g_type = "uint8_t";
		g_width = 2;
		per_line = 10;
	}
	fprintf(nbperf->output, "\tstatic const %s g[%" PRId32 "] = {\n",
	    g_type, state->graph.v);
	for (i = 0; i < state->graph.v; ++i) {
		fprintf(nbperf->output, "%s0x%0*" PRIx32 ",%s",
		    (i % per_line == 0 ? "\t    " : " "),
		    g_width, state->g[i],
		    (i % per_line == per_line - 1 ? "\n" : ""));
	}
	if (i % per_line != 0)
		fprintf(nbperf->output, "\n\t};\n");
	else
		fprintf(nbperf->output, "\t};\n");
	fprintf(nbperf->output, "\tuint32_t h[%zu];\n\n", nbperf->hash_size);
	(*nbperf->print_hash)(nbperf, "\t", "key", "keylen", "h");
#ifdef BUILD_CHM3
	fprintf(nbperf->output, "\treturn (g[h[0] %% %" PRIu32 "] + "
	    "g[h[1] %% %" PRIu32 "] + "
	    "g[h[2] %% %" PRIu32"]) %% %" PRIu32 ";\n",
	    state->graph.v, state->graph.v, state->graph.v, state->graph.e);
#else
	fprintf(nbperf->output, "\treturn (g[h[0] %% %" PRIu32 "] + "
	    "g[h[1] %% %" PRIu32"]) %% %" PRIu32 ";\n",
	    state->graph.v, state->graph.v, state->graph.e);
#endif
	fprintf(nbperf->output, "}\n");

	if (nbperf->map_output != NULL) {
		for (i = 0; i < state->graph.e; ++i)
			fprintf(nbperf->map_output, "%" PRIu32 "\n", i);
	}
}

int
#ifdef BUILD_CHM3
chm3_compute(struct nbperf *nbperf)
#else
chm_compute(struct nbperf *nbperf)
#endif
{
	struct state state;
	int retval = -1;
	uint32_t v, e;

#ifdef BUILD_CHM3
	if (nbperf->c == 0)
		nbperf-> c = 1.24;

	if (nbperf->c < 1.24)
		errx(1, "The argument for option -c must be at least 1.24");

	if (nbperf->hash_size < 3)
		errx(1, "The hash function must generate at least 3 values");
#else
	if (nbperf->c == 0)
		nbperf-> c = 2;

	if (nbperf->c < 2)
		errx(1, "The argument for option -c must be at least 2");

	if (nbperf->hash_size < 2)
		errx(1, "The hash function must generate at least 2 values");
#endif

	(*nbperf->seed_hash)(nbperf);
	e = nbperf->n;
	v = nbperf->c * nbperf->n;
#ifdef BUILD_CHM3
	if (v == 1.24 * nbperf->n)
		++v;
	if (v < 10)
		v = 10;
#else
	if (v == 2 * nbperf->n)
		++v;
#endif

	state.g = calloc(sizeof(uint32_t), v);
	state.visited = calloc(sizeof(uint8_t), v);
	if (state.g == NULL || state.visited == NULL)
		err(1, "malloc failed");

#ifdef BUILD_CHM3
	graph3_setup(&state.graph, v, e);
	if (graph3_hash(nbperf, &state.graph))
		goto failed;
	if (graph3_output_order(&state.graph))
		goto failed;
#else
	graph2_setup(&state.graph, v, e);
	if (graph2_hash(nbperf, &state.graph))
		goto failed;
	if (graph2_output_order(&state.graph))
		goto failed;
#endif
	assign_nodes(&state);
	print_hash(nbperf, &state);

	retval = 0;

failed:
#ifdef BUILD_CHM3
	graph3_free(&state.graph);
#else
	graph2_free(&state.graph);
#endif
	free(state.g);
	free(state.visited);
	return retval;
}
