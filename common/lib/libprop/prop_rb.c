/*	$NetBSD: prop_rb.c,v 1.9 2008/06/17 21:29:47 thorpej Exp $	*/

/*-
 * Copyright (c) 2001 The NetBSD Foundation, Inc.
 * All rights reserved.
 *
 * This code is derived from software contributed to The NetBSD Foundation
 * by Matt Thomas <matt@3am-software.com>.
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

#include <prop/proplib.h>

#include "prop_object_impl.h"
#include "prop_rb_impl.h"

#undef KASSERT
#ifdef RBDEBUG
#define	KASSERT(x)	_PROP_ASSERT(x)
#else
#define	KASSERT(x)	/* nothing */
#endif

#ifndef __predict_false
#define	__predict_false(x)	(x)
#endif

static void rb_tree_reparent_nodes(struct rb_tree *, struct rb_node *,
				   unsigned int);
static void rb_tree_insert_rebalance(struct rb_tree *, struct rb_node *);
static void rb_tree_removal_rebalance(struct rb_tree *, struct rb_node *,
	unsigned int);
#ifdef RBDEBUG
static const struct rb_node *rb_tree_iterate_const(const struct rb_tree *,
	const struct rb_node *, unsigned int);
static bool rb_tree_check_node(const struct rb_tree *, const struct rb_node *,
	const struct rb_node *, bool);
#endif

#ifdef RBDEBUG
#define	RBT_COUNT_INCR(rbt)	(rbt)->rbt_count++
#define	RBT_COUNT_DECR(rbt)	(rbt)->rbt_count--
#else
#define	RBT_COUNT_INCR(rbt)	/* nothing */
#define	RBT_COUNT_DECR(rbt)	/* nothing */
#endif

#define	RBUNCONST(a)	((void *)(unsigned long)(const void *)(a))

/*
 * Rather than testing for the NULL everywhere, all terminal leaves are
 * pointed to this node (and that includes itself).  Note that by setting
 * it to be const, that on some architectures trying to write to it will
 * cause a fault.
 */
static const struct rb_node sentinel_node = {
	.rb_nodes = { RBUNCONST(&sentinel_node),
		      RBUNCONST(&sentinel_node),
		      NULL },
	.rb_u = { .u_s = { .s_sentinel = 1 } },
};

void
_prop_rb_tree_init(struct rb_tree *rbt, const struct rb_tree_ops *ops)
{
	RB_TAILQ_INIT(&rbt->rbt_nodes);
#ifdef RBDEBUG
	rbt->rbt_count = 0;
#endif
	rbt->rbt_ops = ops;
	*((const struct rb_node **)&rbt->rbt_root) = &sentinel_node;
}

/*
 * Swap the location and colors of 'self' and its child @ which.  The child
 * can not be a sentinel node.
 */
/*ARGSUSED*/
static void
rb_tree_reparent_nodes(struct rb_tree *rbt _PROP_ARG_UNUSED,
    struct rb_node *old_father, unsigned int which)
{
	const unsigned int other = which ^ RB_NODE_OTHER;
	struct rb_node * const grandpa = old_father->rb_parent;
	struct rb_node * const old_child = old_father->rb_nodes[which];
	struct rb_node * const new_father = old_child;
	struct rb_node * const new_child = old_father;
	unsigned int properties;

	KASSERT(which == RB_NODE_LEFT || which == RB_NODE_RIGHT);

	KASSERT(!RB_SENTINEL_P(old_child));
	KASSERT(old_child->rb_parent == old_father);

	KASSERT(rb_tree_check_node(rbt, old_father, NULL, false));
	KASSERT(rb_tree_check_node(rbt, old_child, NULL, false));
	KASSERT(RB_ROOT_P(old_father) || rb_tree_check_node(rbt, grandpa, NULL, false));

	/*
	 * Exchange descendant linkages.
	 */
	grandpa->rb_nodes[old_father->rb_position] = new_father;
	new_child->rb_nodes[which] = old_child->rb_nodes[other];
	new_father->rb_nodes[other] = new_child;

	/*
	 * Update ancestor linkages
	 */
	new_father->rb_parent = grandpa;
	new_child->rb_parent = new_father;

	/*
	 * Exchange properties between new_father and new_child.  The only
	 * change is that new_child's position is now on the other side.
	 */
	properties = old_child->rb_properties;
	new_father->rb_properties = old_father->rb_properties;
	new_child->rb_properties = properties;
	new_child->rb_position = other;

	/*
	 * Make sure to reparent the new child to ourself.
	 */
	if (!RB_SENTINEL_P(new_child->rb_nodes[which])) {
		new_child->rb_nodes[which]->rb_parent = new_child;
		new_child->rb_nodes[which]->rb_position = which;
	}

	KASSERT(rb_tree_check_node(rbt, new_father, NULL, false));
	KASSERT(rb_tree_check_node(rbt, new_child, NULL, false));
	KASSERT(RB_ROOT_P(new_father) || rb_tree_check_node(rbt, grandpa, NULL, false));
}

bool
_prop_rb_tree_insert_node(struct rb_tree *rbt, struct rb_node *self)
{
	struct rb_node *parent, *tmp;
	rb_compare_nodes_fn compare_nodes = rbt->rbt_ops->rbto_compare_nodes;
	unsigned int position;

	self->rb_properties = 0;
	tmp = rbt->rbt_root;
	/*
	 * This is a hack.  Because rbt->rbt_root is just a struct rb_node *,
	 * just like rb_node->rb_nodes[RB_NODE_LEFT], we can use this fact to
	 * avoid a lot of tests for root and know that even at root,
	 * updating rb_node->rb_parent->rb_nodes[rb_node->rb_position] will
	 * rbt->rbt_root.
	 */
	/* LINTED: see above */
	parent = (struct rb_node *)&rbt->rbt_root;
	position = RB_NODE_LEFT;

	/*
	 * Find out where to place this new leaf.
	 */
	while (!RB_SENTINEL_P(tmp)) {
		const int diff = (*compare_nodes)(tmp, self);
		if (__predict_false(diff == 0)) {
			/*
			 * Node already exists; don't insert.
			 */
			return false;
		}
		parent = tmp;
		KASSERT(diff != 0);
		if (diff < 0) {
			position = RB_NODE_LEFT;
		} else {
			position = RB_NODE_RIGHT;
		}
		tmp = parent->rb_nodes[position];
	}

#ifdef RBDEBUG
	{
		struct rb_node *prev = NULL, *next = NULL;

		if (position == RB_NODE_RIGHT)
			prev = parent;
		else if (tmp != rbt->rbt_root)
			next = parent;

		/*
		 * Verify our sequential position
		 */
		KASSERT(prev == NULL || !RB_SENTINEL_P(prev));
		KASSERT(next == NULL || !RB_SENTINEL_P(next));
		if (prev != NULL && next == NULL)
			next = TAILQ_NEXT(prev, rb_link);
		if (prev == NULL && next != NULL)
			prev = TAILQ_PREV(next, rb_node_qh, rb_link);
		KASSERT(prev == NULL || !RB_SENTINEL_P(prev));
		KASSERT(next == NULL || !RB_SENTINEL_P(next));
		KASSERT(prev == NULL
			|| (*compare_nodes)(prev, self) > 0);
		KASSERT(next == NULL
			|| (*compare_nodes)(self, next) > 0);
	}
#endif

	/*
	 * Initialize the node and insert as a leaf into the tree.
	 */
	self->rb_parent = parent;
	self->rb_position = position;
	/* LINTED: rbt_root hack */
	if (__predict_false(parent == (struct rb_node *) &rbt->rbt_root)) {
		RB_MARK_ROOT(self);
	} else {
		KASSERT(position == RB_NODE_LEFT || position == RB_NODE_RIGHT);
		KASSERT(!RB_ROOT_P(self)); 	/* Already done */
	}
	KASSERT(RB_SENTINEL_P(parent->rb_nodes[position]));
	self->rb_left = parent->rb_nodes[position];
	self->rb_right = parent->rb_nodes[position];
	parent->rb_nodes[position] = self;
	KASSERT(self->rb_left == &sentinel_node &&
	    self->rb_right == &sentinel_node);

	/*
	 * Insert the new node into a sorted list for easy sequential access
	 */
	RBT_COUNT_INCR(rbt);
#ifdef RBDEBUG
	if (RB_ROOT_P(self)) {
		RB_TAILQ_INSERT_HEAD(&rbt->rbt_nodes, self, rb_link);
	} else if (position == RB_NODE_LEFT) {
		KASSERT((*compare_nodes)(self, self->rb_parent) > 0);
		RB_TAILQ_INSERT_BEFORE(self->rb_parent, self, rb_link);
	} else {
		KASSERT((*compare_nodes)(self->rb_parent, self) > 0);
		RB_TAILQ_INSERT_AFTER(&rbt->rbt_nodes, self->rb_parent,
		    self, rb_link);
	}
#endif

#if 0
	/*
	 * Validate the tree before we rebalance
	 */
	_prop_rb_tree_check(rbt, false);
#endif

	/*
	 * Rebalance tree after insertion
	 */
	rb_tree_insert_rebalance(rbt, self);

#if 0
	/*
	 * Validate the tree after we rebalanced
	 */
	_prop_rb_tree_check(rbt, true);
#endif

	return true;
}

static void
rb_tree_insert_rebalance(struct rb_tree *rbt, struct rb_node *self)
{
	RB_MARK_RED(self);

	while (!RB_ROOT_P(self) && RB_RED_P(self->rb_parent)) {
		const unsigned int which =
		     (self->rb_parent == self->rb_parent->rb_parent->rb_left
			? RB_NODE_LEFT
			: RB_NODE_RIGHT);
		const unsigned int other = which ^ RB_NODE_OTHER;
		struct rb_node * father = self->rb_parent;
		struct rb_node * grandpa = father->rb_parent;
		struct rb_node * const uncle = grandpa->rb_nodes[other];

		KASSERT(!RB_SENTINEL_P(self));
		/*
		 * We are red and our parent is red, therefore we must have a
		 * grandfather and he must be black.
		 */
		KASSERT(RB_RED_P(self)
			&& RB_RED_P(father)
			&& RB_BLACK_P(grandpa));

		if (RB_RED_P(uncle)) {
			/*
			 * Case 1: our uncle is red
			 *   Simply invert the colors of our parent and
			 *   uncle and make our grandparent red.  And
			 *   then solve the problem up at his level.
			 */
			RB_MARK_BLACK(uncle);
			RB_MARK_BLACK(father);
			RB_MARK_RED(grandpa);
			self = grandpa;
			continue;
		}
		/*
		 * Case 2&3: our uncle is black.
		 */
		if (self == father->rb_nodes[other]) {
			/*
			 * Case 2: we are on the same side as our uncle
			 *   Swap ourselves with our parent so this case
			 *   becomes case 3.  Basically our parent becomes our
			 *   child.
			 */
			rb_tree_reparent_nodes(rbt, father, other);
			KASSERT(father->rb_parent == self);
			KASSERT(self->rb_nodes[which] == father);
			KASSERT(self->rb_parent == grandpa);
			self = father;
			father = self->rb_parent;
		}
		KASSERT(RB_RED_P(self) && RB_RED_P(father));
		KASSERT(grandpa->rb_nodes[which] == father);
		/*
		 * Case 3: we are opposite a child of a black uncle.
		 *   Swap our parent and grandparent.  Since our grandfather
		 *   is black, our father will become black and our new sibling
		 *   (former grandparent) will become red.
		 */
		rb_tree_reparent_nodes(rbt, grandpa, which);
		KASSERT(self->rb_parent == father);
		KASSERT(self->rb_parent->rb_nodes[self->rb_position ^ RB_NODE_OTHER] == grandpa);
		KASSERT(RB_RED_P(self));
		KASSERT(RB_BLACK_P(father));
		KASSERT(RB_RED_P(grandpa));
		break;
	}

	/*
	 * Final step: Set the root to black.
	 */
	RB_MARK_BLACK(rbt->rbt_root);
}

struct rb_node *
_prop_rb_tree_find(struct rb_tree *rbt, const void *key)
{
	struct rb_node *parent = rbt->rbt_root;
	rb_compare_key_fn compare_key = rbt->rbt_ops->rbto_compare_key;

	while (!RB_SENTINEL_P(parent)) {
		const int diff = (*compare_key)(parent, key);
		if (diff == 0)
			return parent;
		parent = parent->rb_nodes[diff > 0];
	}

	return NULL;
}

static void
rb_tree_prune_node(struct rb_tree *rbt, struct rb_node *self, int rebalance)
{
	const unsigned int which = self->rb_position;
	struct rb_node *father = self->rb_parent;

	KASSERT(rebalance || (RB_ROOT_P(self) || RB_RED_P(self)));
	KASSERT(!rebalance || RB_BLACK_P(self));
	KASSERT(RB_CHILDLESS_P(self));
	KASSERT(rb_tree_check_node(rbt, self, NULL, false));

	father->rb_nodes[which] = self->rb_left;

	/*
	 * Remove ourselves from the node list and decrement the count.
	 */
	RB_TAILQ_REMOVE(&rbt->rbt_nodes, self, rb_link);
	RBT_COUNT_DECR(rbt);

	if (rebalance)
		rb_tree_removal_rebalance(rbt, father, which);
	KASSERT(RB_ROOT_P(self) || rb_tree_check_node(rbt, father, NULL, true));
}

static void
rb_tree_swap_prune_and_rebalance(struct rb_tree *rbt, struct rb_node *self,
	struct rb_node *standin)
{
	unsigned int standin_which = standin->rb_position;
	unsigned int standin_other = standin_which ^ RB_NODE_OTHER;
	struct rb_node *standin_child;
	struct rb_node *standin_father;
	bool rebalance = RB_BLACK_P(standin);

	if (standin->rb_parent == self) {
		/*
		 * As a child of self, any childen would be opposite of
		 * our parent (self).
		 */
		KASSERT(RB_SENTINEL_P(standin->rb_nodes[standin_other]));
		standin_child = standin->rb_nodes[standin_which];
	} else {
		/*
		 * Since we aren't a child of self, any childen would be
		 * on the same side as our parent (self).
		 */
		KASSERT(RB_SENTINEL_P(standin->rb_nodes[standin_which]));
		standin_child = standin->rb_nodes[standin_other];
	}

	/*
	 * the node we are removing must have two children.
	 */
	KASSERT(RB_TWOCHILDREN_P(self));
	/*
	 * If standin has a child, it must be red.
	 */
	KASSERT(RB_SENTINEL_P(standin_child) || RB_RED_P(standin_child));

	/*
	 * Verify things are sane.
	 */
	KASSERT(rb_tree_check_node(rbt, self, NULL, false));
	KASSERT(rb_tree_check_node(rbt, standin, NULL, false));

	if (!RB_SENTINEL_P(standin_child)) {
		/*
		 * We know we have a red child so if we swap them we can
		 * void flipping standin's child to black afterwards.
		 */
		KASSERT(rb_tree_check_node(rbt, standin_child, NULL, true));
		rb_tree_reparent_nodes(rbt, standin,
		    standin_child->rb_position);
		KASSERT(rb_tree_check_node(rbt, standin, NULL, true));
		KASSERT(rb_tree_check_node(rbt, standin_child, NULL, true));
		/*
		 * Since we are removing a red leaf, no need to rebalance.
		 */
		rebalance = false;
		/*
		 * We know that standin can not be a child of self, so
		 * update before of that.
		 */
		KASSERT(standin->rb_parent != self);
		standin_which = standin->rb_position;
		standin_other = standin_which ^ RB_NODE_OTHER;
	}
	KASSERT(RB_CHILDLESS_P(standin));

	/*
	 * If we are about to delete the standin's father, then when we call
	 * rebalance, we need to use ourselves as our father.  Otherwise
	 * remember our original father.  Also, if we are our standin's father
	 * we only need to reparent the standin's brother.
	 */
	if (standin->rb_parent == self) {
		/*
		 * |   R   -->   S   |
		 * | Q   S --> Q   * |
		 * |       -->       |
		 */
		standin_father = standin;
		KASSERT(RB_SENTINEL_P(standin->rb_nodes[standin_other]));
		KASSERT(!RB_SENTINEL_P(self->rb_nodes[standin_other]));
		KASSERT(self->rb_nodes[standin_which] == standin);
		/*
		 * Make our brother our son.
		 */
		standin->rb_nodes[standin_other] = self->rb_nodes[standin_other];
		standin->rb_nodes[standin_other]->rb_parent = standin;
		KASSERT(standin->rb_nodes[standin_other]->rb_position == standin_other);
	} else {
		/*
		 * |  P      -->  P    |
		 * |      S  -->    Q  |
		 * |    Q    -->       |
		 */
		standin_father = standin->rb_parent;
		standin_father->rb_nodes[standin_which] =
		    standin->rb_nodes[standin_which];
		standin->rb_left = self->rb_left;
		standin->rb_right = self->rb_right;
		standin->rb_left->rb_parent = standin;
		standin->rb_right->rb_parent = standin;
	}

	/*
	 * Now copy the result of self to standin and then replace
	 * self with standin in the tree.
	 */
	standin->rb_parent = self->rb_parent;
	standin->rb_properties = self->rb_properties;
	standin->rb_parent->rb_nodes[standin->rb_position] = standin;

	/*
	 * Remove ourselves from the node list and decrement the count.
	 */
	RB_TAILQ_REMOVE(&rbt->rbt_nodes, self, rb_link);
	RBT_COUNT_DECR(rbt);

	KASSERT(rb_tree_check_node(rbt, standin, NULL, false));
	KASSERT(rb_tree_check_node(rbt, standin_father, NULL, false));

	if (!rebalance)
		return;

	rb_tree_removal_rebalance(rbt, standin_father, standin_which);
	KASSERT(rb_tree_check_node(rbt, standin, NULL, true));
}

/*
 * We could do this by doing
 *	rb_tree_node_swap(rbt, self, which);
 *	rb_tree_prune_node(rbt, self, false);
 *
 * But it's more efficient to just evalate and recolor the child.
 */
/*ARGSUSED*/
static void
rb_tree_prune_blackred_branch(struct rb_tree *rbt _PROP_ARG_UNUSED,
    struct rb_node *self, unsigned int which)
{
	struct rb_node *parent = self->rb_parent;
	struct rb_node *child = self->rb_nodes[which];

	KASSERT(which == RB_NODE_LEFT || which == RB_NODE_RIGHT);
	KASSERT(RB_BLACK_P(self) && RB_RED_P(child));
	KASSERT(!RB_TWOCHILDREN_P(child));
	KASSERT(RB_CHILDLESS_P(child));
	KASSERT(rb_tree_check_node(rbt, self, NULL, false));
	KASSERT(rb_tree_check_node(rbt, child, NULL, false));

	/*
	 * Remove ourselves from the tree and give our former child our
	 * properties (position, color, root).
	 */
	parent->rb_nodes[self->rb_position] = child;
	child->rb_parent = parent;
	child->rb_properties = self->rb_properties;

	/*
	 * Remove ourselves from the node list and decrement the count.
	 */
	RB_TAILQ_REMOVE(&rbt->rbt_nodes, self, rb_link);
	RBT_COUNT_DECR(rbt);

	KASSERT(RB_ROOT_P(self) || rb_tree_check_node(rbt, parent, NULL, true));
	KASSERT(rb_tree_check_node(rbt, child, NULL, true));
}
/*
 *
 */
void
_prop_rb_tree_remove_node(struct rb_tree *rbt, struct rb_node *self)
{
	struct rb_node *standin;
	unsigned int which;
	/*
	 * In the following diagrams, we (the node to be removed) are S.  Red
	 * nodes are lowercase.  T could be either red or black.
	 *
	 * Remember the major axiom of the red-black tree: the number of
	 * black nodes from the root to each leaf is constant across all
	 * leaves, only the number of red nodes varies.
	 *
	 * Thus removing a red leaf doesn't require any other changes to a
	 * red-black tree.  So if we must remove a node, attempt to rearrange
	 * the tree so we can remove a red node.
	 *
	 * The simpliest case is a childless red node or a childless root node:
	 *
	 * |    T  -->    T  |    or    |  R  -->  *  |
	 * |  s    -->  *    |
	 */
	if (RB_CHILDLESS_P(self)) {
		if (RB_RED_P(self) || RB_ROOT_P(self)) {
			rb_tree_prune_node(rbt, self, false);
			return;
		}
		rb_tree_prune_node(rbt, self, true);
		return;
	}
	KASSERT(!RB_CHILDLESS_P(self));
	if (!RB_TWOCHILDREN_P(self)) {
		/*
		 * The next simpliest case is the node we are deleting is
		 * black and has one red child.
		 *
		 * |      T  -->      T  -->      T  |
		 * |    S    -->  R      -->  R      |
		 * |  r      -->    s    -->    *    |
		 */
		which = RB_LEFT_SENTINEL_P(self) ? RB_NODE_RIGHT : RB_NODE_LEFT;
		KASSERT(RB_BLACK_P(self));
		KASSERT(RB_RED_P(self->rb_nodes[which]));
		KASSERT(RB_CHILDLESS_P(self->rb_nodes[which]));
		rb_tree_prune_blackred_branch(rbt, self, which);
		return;
	}
	KASSERT(RB_TWOCHILDREN_P(self));

	/*
	 * We invert these because we prefer to remove from the inside of
	 * the tree.
	 */
	which = self->rb_position ^ RB_NODE_OTHER;

	/*
	 * Let's find the node closes to us opposite of our parent
	 * Now swap it with ourself, "prune" it, and rebalance, if needed.
	 */
	standin = _prop_rb_tree_iterate(rbt, self, which);
	rb_tree_swap_prune_and_rebalance(rbt, self, standin);
}

static void
rb_tree_removal_rebalance(struct rb_tree *rbt, struct rb_node *parent,
	unsigned int which)
{
	KASSERT(!RB_SENTINEL_P(parent));
	KASSERT(RB_SENTINEL_P(parent->rb_nodes[which]));
	KASSERT(which == RB_NODE_LEFT || which == RB_NODE_RIGHT);

	while (RB_BLACK_P(parent->rb_nodes[which])) {
		unsigned int other = which ^ RB_NODE_OTHER;
		struct rb_node *brother = parent->rb_nodes[other];

		KASSERT(!RB_SENTINEL_P(brother));
		/*
		 * For cases 1, 2a, and 2b, our brother's children must
		 * be black and our father must be black
		 */
		if (RB_BLACK_P(parent)
		    && RB_BLACK_P(brother->rb_left)
		    && RB_BLACK_P(brother->rb_right)) {
			/*
			 * Case 1: Our brother is red, swap its position
			 * (and colors) with our parent.  This is now case 2b.
			 *
			 *    B         ->        D
			 *  x     d     ->    b     E
			 *      C   E   ->  x   C
			 */
			if (RB_RED_P(brother)) {
				KASSERT(RB_BLACK_P(parent));
				rb_tree_reparent_nodes(rbt, parent, other);
				brother = parent->rb_nodes[other];
				KASSERT(!RB_SENTINEL_P(brother));
				KASSERT(RB_BLACK_P(brother));
				KASSERT(RB_RED_P(parent));
				KASSERT(rb_tree_check_node(rbt, brother, NULL, false));
				KASSERT(rb_tree_check_node(rbt, parent, NULL, false));
			} else {
				/*
				 * Both our parent and brother are black.
				 * Change our brother to red, advance up rank
				 * and go through the loop again.
				 *
				 *    B         ->    B
				 *  A     D     ->  A     d
				 *      C   E   ->      C   E
				 */
				RB_MARK_RED(brother);
				KASSERT(RB_BLACK_P(brother->rb_left));
				KASSERT(RB_BLACK_P(brother->rb_right));
				if (RB_ROOT_P(parent))
					return;
				KASSERT(rb_tree_check_node(rbt, brother, NULL, false));
				KASSERT(rb_tree_check_node(rbt, parent, NULL, false));
				which = parent->rb_position;
				parent = parent->rb_parent;
			}
		} else if (RB_RED_P(parent)
		    && RB_BLACK_P(brother)
		    && RB_BLACK_P(brother->rb_left)
		    && RB_BLACK_P(brother->rb_right)) {
			KASSERT(RB_BLACK_P(brother));
			KASSERT(RB_BLACK_P(brother->rb_left));
			KASSERT(RB_BLACK_P(brother->rb_right));
			RB_MARK_BLACK(parent);
			RB_MARK_RED(brother);
			KASSERT(rb_tree_check_node(rbt, brother, NULL, true));
			break;		/* We're done! */
		} else {
			KASSERT(RB_BLACK_P(brother));
			KASSERT(!RB_CHILDLESS_P(brother));
			/*
			 * Case 3: our brother is black, our left nephew is
			 * red, and our right nephew is black.  Swap our
			 * brother with our left nephew.   This result in a
			 * tree that matches case 4.
			 *
			 *     B         ->       D
			 * A       D     ->   B     E
			 *       c   e   -> A   C
			 */
			if (RB_BLACK_P(brother->rb_nodes[other])) {
				KASSERT(RB_RED_P(brother->rb_nodes[which]));
				rb_tree_reparent_nodes(rbt, brother, which);
				KASSERT(brother->rb_parent == parent->rb_nodes[other]);
				brother = parent->rb_nodes[other];
				KASSERT(RB_RED_P(brother->rb_nodes[other]));
			}
			/*
			 * Case 4: our brother is black and our right nephew
			 * is red.  Swap our parent and brother locations and
			 * change our right nephew to black.  (these can be
			 * done in either order so we change the color first).
			 * The result is a valid red-black tree and is a
			 * terminal case.
			 *
			 *     B         ->       D
			 * A       D     ->   B     E
			 *       c   e   -> A   C
			 */
			RB_MARK_BLACK(brother->rb_nodes[other]);
			rb_tree_reparent_nodes(rbt, parent, other);
			break;		/* We're done! */
		}
	}
	KASSERT(rb_tree_check_node(rbt, parent, NULL, true));
}

struct rb_node *
_prop_rb_tree_iterate(struct rb_tree *rbt, struct rb_node *self,
	unsigned int direction)
{
	const unsigned int other = direction ^ RB_NODE_OTHER;
	KASSERT(direction == RB_NODE_LEFT || direction == RB_NODE_RIGHT);

	if (self == NULL) {
		self = rbt->rbt_root;
		if (RB_SENTINEL_P(self))
			return NULL;
		while (!RB_SENTINEL_P(self->rb_nodes[other]))
			self = self->rb_nodes[other];
		return self;
	}
	KASSERT(!RB_SENTINEL_P(self));
	/*
	 * We can't go any further in this direction.  We proceed up in the
	 * opposite direction until our parent is in direction we want to go.
	 */
	if (RB_SENTINEL_P(self->rb_nodes[direction])) {
		while (!RB_ROOT_P(self)) {
			if (other == self->rb_position)
				return self->rb_parent;
			self = self->rb_parent;
		}
		return NULL;
	}

	/*
	 * Advance down one in current direction and go down as far as possible
	 * in the opposite direction.
	 */
	self = self->rb_nodes[direction];
	KASSERT(!RB_SENTINEL_P(self));
	while (!RB_SENTINEL_P(self->rb_nodes[other]))
		self = self->rb_nodes[other];
	return self;
}

#ifdef RBDEBUG
static const struct rb_node *
rb_tree_iterate_const(const struct rb_tree *rbt, const struct rb_node *self,
	unsigned int direction)
{
	const unsigned int other = direction ^ RB_NODE_OTHER;
	KASSERT(direction == RB_NODE_LEFT || direction == RB_NODE_RIGHT);

	if (self == NULL) {
		self = rbt->rbt_root;
		if (RB_SENTINEL_P(self))
			return NULL;
		while (!RB_SENTINEL_P(self->rb_nodes[other]))
			self = self->rb_nodes[other];
		return self;
	}
	KASSERT(!RB_SENTINEL_P(self));
	/*
	 * We can't go any further in this direction.  We proceed up in the
	 * opposite direction until our parent is in direction we want to go.
	 */
	if (RB_SENTINEL_P(self->rb_nodes[direction])) {
		while (!RB_ROOT_P(self)) {
			if (other == self->rb_position)
				return self->rb_parent;
			self = self->rb_parent;
		}
		return NULL;
	}

	/*
	 * Advance down one in current direction and go down as far as possible
	 * in the opposite direction.
	 */
	self = self->rb_nodes[direction];
	KASSERT(!RB_SENTINEL_P(self));
	while (!RB_SENTINEL_P(self->rb_nodes[other]))
		self = self->rb_nodes[other];
	return self;
}

static bool
rb_tree_check_node(const struct rb_tree *rbt, const struct rb_node *self,
	const struct rb_node *prev, bool red_check)
{
	KASSERT(!self->rb_sentinel);
	KASSERT(self->rb_left);
	KASSERT(self->rb_right);
	KASSERT(prev == NULL ||
		(*rbt->rbt_ops->rbto_compare_nodes)(prev, self) > 0);

	/*
	 * Verify our relationship to our parent.
	 */
	if (RB_ROOT_P(self)) {
		KASSERT(self == rbt->rbt_root);
		KASSERT(self->rb_position == RB_NODE_LEFT);
		KASSERT(self->rb_parent->rb_nodes[RB_NODE_LEFT] == self);
		KASSERT(self->rb_parent == (const struct rb_node *) &rbt->rbt_root);
	} else {
		KASSERT(self != rbt->rbt_root);
		KASSERT(!RB_PARENT_SENTINEL_P(self));
		if (self->rb_position == RB_NODE_LEFT) {
			KASSERT((*rbt->rbt_ops->rbto_compare_nodes)(self, self->rb_parent) > 0);
			KASSERT(self->rb_parent->rb_nodes[RB_NODE_LEFT] == self);
		} else {
			KASSERT((*rbt->rbt_ops->rbto_compare_nodes)(self, self->rb_parent) < 0);
			KASSERT(self->rb_parent->rb_nodes[RB_NODE_RIGHT] == self);
		}
	}

	/*
	 * Verify our position in the linked list against the tree itself.
	 */
	{
		const struct rb_node *prev0 = rb_tree_iterate_const(rbt, self, RB_NODE_LEFT);
		const struct rb_node *next0 = rb_tree_iterate_const(rbt, self, RB_NODE_RIGHT);
		KASSERT(prev0 == TAILQ_PREV(self, rb_node_qh, rb_link));
		if (next0 != TAILQ_NEXT(self, rb_link))
			next0 = rb_tree_iterate_const(rbt, self, RB_NODE_RIGHT);
		KASSERT(next0 == TAILQ_NEXT(self, rb_link));
	}

	/*
	 * The root must be black.
	 * There can never be two adjacent red nodes. 
	 */
	if (red_check) {
		KASSERT(!RB_ROOT_P(self) || RB_BLACK_P(self));
		if (RB_RED_P(self)) {
			const struct rb_node *brother;
			KASSERT(!RB_ROOT_P(self));
			brother = self->rb_parent->rb_nodes[self->rb_position ^ RB_NODE_OTHER];
			KASSERT(RB_BLACK_P(self->rb_parent));
			/* 
			 * I'm red and have no children, then I must either
			 * have no brother or my brother also be red and
			 * also have no children.  (black count == 0)
			 */
			KASSERT(!RB_CHILDLESS_P(self)
				|| RB_SENTINEL_P(brother)
				|| RB_RED_P(brother)
				|| RB_CHILDLESS_P(brother));
			/*
			 * If I'm not childless, I must have two children
			 * and they must be both be black.
			 */
			KASSERT(RB_CHILDLESS_P(self)
				|| (RB_TWOCHILDREN_P(self)
				    && RB_BLACK_P(self->rb_left)
				    && RB_BLACK_P(self->rb_right)));
			/*
			 * If I'm not childless, thus I have black children,
			 * then my brother must either be black or have two
			 * black children.
			 */
			KASSERT(RB_CHILDLESS_P(self)
				|| RB_BLACK_P(brother)
				|| (RB_TWOCHILDREN_P(brother)
				    && RB_BLACK_P(brother->rb_left)
				    && RB_BLACK_P(brother->rb_right)));
		} else {
			/*
			 * If I'm black and have one child, that child must
			 * be red and childless.
			 */
			KASSERT(RB_CHILDLESS_P(self)
				|| RB_TWOCHILDREN_P(self)
				|| (!RB_LEFT_SENTINEL_P(self)
				    && RB_RIGHT_SENTINEL_P(self)
				    && RB_RED_P(self->rb_left)
				    && RB_CHILDLESS_P(self->rb_left))
				|| (!RB_RIGHT_SENTINEL_P(self)
				    && RB_LEFT_SENTINEL_P(self)
				    && RB_RED_P(self->rb_right)
				    && RB_CHILDLESS_P(self->rb_right)));

			/*
			 * If I'm a childless black node and my parent is
			 * black, my 2nd closet relative away from my parent
			 * is either red or has a red parent or red children.
			 */
			if (!RB_ROOT_P(self)
			    && RB_CHILDLESS_P(self)
			    && RB_BLACK_P(self->rb_parent)) {
				const unsigned int which = self->rb_position;
				const unsigned int other = which ^ RB_NODE_OTHER;
				const struct rb_node *relative0, *relative;

				relative0 = rb_tree_iterate_const(rbt,
				    self, other);
				KASSERT(relative0 != NULL);
				relative = rb_tree_iterate_const(rbt,
				    relative0, other);
				KASSERT(relative != NULL);
				KASSERT(RB_SENTINEL_P(relative->rb_nodes[which]));
#if 0
				KASSERT(RB_RED_P(relative)
					|| RB_RED_P(relative->rb_left)
					|| RB_RED_P(relative->rb_right)
					|| RB_RED_P(relative->rb_parent));
#endif
			}
		}
		/*
		 * A grandparent's children must be real nodes and not
		 * sentinels.  First check out grandparent.
		 */
		KASSERT(RB_ROOT_P(self)
			|| RB_ROOT_P(self->rb_parent)
			|| RB_TWOCHILDREN_P(self->rb_parent->rb_parent));
		/*
		 * If we are have grandchildren on our left, then
		 * we must have a child on our right.
		 */
		KASSERT(RB_LEFT_SENTINEL_P(self)
			|| RB_CHILDLESS_P(self->rb_left)
			|| !RB_RIGHT_SENTINEL_P(self));
		/*
		 * If we are have grandchildren on our right, then
		 * we must have a child on our left.
		 */
		KASSERT(RB_RIGHT_SENTINEL_P(self)
			|| RB_CHILDLESS_P(self->rb_right)
			|| !RB_LEFT_SENTINEL_P(self));

		/*
		 * If we have a child on the left and it doesn't have two
		 * children make sure we don't have great-great-grandchildren on
		 * the right.
		 */
		KASSERT(RB_TWOCHILDREN_P(self->rb_left)
			|| RB_CHILDLESS_P(self->rb_right)
			|| RB_CHILDLESS_P(self->rb_right->rb_left)
			|| RB_CHILDLESS_P(self->rb_right->rb_left->rb_left)
			|| RB_CHILDLESS_P(self->rb_right->rb_left->rb_right)
			|| RB_CHILDLESS_P(self->rb_right->rb_right)
			|| RB_CHILDLESS_P(self->rb_right->rb_right->rb_left)
			|| RB_CHILDLESS_P(self->rb_right->rb_right->rb_right));

		/*
		 * If we have a child on the right and it doesn't have two
		 * children make sure we don't have great-great-grandchildren on
		 * the left.
		 */
		KASSERT(RB_TWOCHILDREN_P(self->rb_right)
			|| RB_CHILDLESS_P(self->rb_left)
			|| RB_CHILDLESS_P(self->rb_left->rb_left)
			|| RB_CHILDLESS_P(self->rb_left->rb_left->rb_left)
			|| RB_CHILDLESS_P(self->rb_left->rb_left->rb_right)
			|| RB_CHILDLESS_P(self->rb_left->rb_right)
			|| RB_CHILDLESS_P(self->rb_left->rb_right->rb_left)
			|| RB_CHILDLESS_P(self->rb_left->rb_right->rb_right));

		/*
		 * If we are fully interior node, then our predecessors and
		 * successors must have no children in our direction.
		 */
		if (RB_TWOCHILDREN_P(self)) {
			const struct rb_node *prev0;
			const struct rb_node *next0;

			prev0 = rb_tree_iterate_const(rbt, self, RB_NODE_LEFT);
			KASSERT(prev0 != NULL);
			KASSERT(RB_RIGHT_SENTINEL_P(prev0));

			next0 = rb_tree_iterate_const(rbt, self, RB_NODE_RIGHT);
			KASSERT(next0 != NULL);
			KASSERT(RB_LEFT_SENTINEL_P(next0));
		}
	}

	return true;
}

static unsigned int
rb_tree_count_black(const struct rb_node *self)
{
	unsigned int left, right;

	if (RB_SENTINEL_P(self))
		return 0;

	left = rb_tree_count_black(self->rb_left);
	right = rb_tree_count_black(self->rb_right);

	KASSERT(left == right);

	return left + RB_BLACK_P(self);
}

void
_prop_rb_tree_check(const struct rb_tree *rbt, bool red_check)
{
	const struct rb_node *self;
	const struct rb_node *prev;
	unsigned int count;

	KASSERT(rbt->rbt_root == NULL || rbt->rbt_root->rb_position == RB_NODE_LEFT);

	prev = NULL;
	count = 0;
	TAILQ_FOREACH(self, &rbt->rbt_nodes, rb_link) {
		rb_tree_check_node(rbt, self, prev, false);
		count++;
	}
	KASSERT(rbt->rbt_count == count);
	KASSERT(RB_SENTINEL_P(rbt->rbt_root)
		|| rb_tree_count_black(rbt->rbt_root));

	/*
	 * The root must be black.
	 * There can never be two adjacent red nodes. 
	 */
	if (red_check) {
		KASSERT(rbt->rbt_root == NULL || RB_BLACK_P(rbt->rbt_root));
		TAILQ_FOREACH(self, &rbt->rbt_nodes, rb_link) {
			rb_tree_check_node(rbt, self, NULL, true);
		}
	}
}
#endif /* RBDEBUG */
