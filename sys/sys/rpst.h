/*	$NetBSD: rpst.h,v 1.3 2011/04/14 15:31:20 yamt Exp $	*/

/*-
 * Copyright (c)2009 YAMAMOTO Takashi,
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
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE AUTHOR OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 */

#if !defined(_SYS_RPST_H_)
#define	_SYS_RPST_H_

#if defined(_KERNEL) || defined(_STANDALONE)
#include <sys/types.h>
#else /* defined(_KERNEL) || defined(_STANDALONE) */
#include <stdint.h>
#endif /* defined(_KERNEL) || defined(_STANDALONE) */

struct rpst_tree {
	struct rpst_node *t_root;
	unsigned int t_height;
};

struct rpst_node {
	struct rpst_node *n_parent;
	struct rpst_node *n_children[2];
	uint64_t n_y;
	uint64_t n_x;
};

struct rpst_iterator {
	struct rpst_tree *it_tree;
	struct rpst_node *it_cur;
	unsigned int it_idx;
	unsigned int it_level;
	uint64_t it_max_y;
	uint64_t it_min_x;
	uint64_t it_max_x;
};

void rpst_init_tree(struct rpst_tree *);
struct rpst_node *rpst_insert_node(struct rpst_tree *, struct rpst_node *);
void rpst_remove_node(struct rpst_tree *, struct rpst_node *);
struct rpst_node *rpst_iterate_first(struct rpst_tree *, uint64_t, uint64_t,
    uint64_t, struct rpst_iterator *);
struct rpst_node *rpst_iterate_next(struct rpst_iterator *);

#endif /* !defined(_SYS_RPST_H_) */
