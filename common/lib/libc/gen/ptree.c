/*	$NetBSD: ptree.c,v 1.10 2012/10/06 22:15:09 matt Exp $	*/

/*-
 * Copyright (c) 2008 The NetBSD Foundation, Inc.
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

#define _PT_PRIVATE

#if defined(PTCHECK) && !defined(PTDEBUG)
#define PTDEBUG
#endif

#if defined(_KERNEL) || defined(_STANDALONE)
#include <sys/param.h>
#include <sys/types.h>
#include <sys/systm.h>
#include <lib/libkern/libkern.h>
__KERNEL_RCSID(0, "$NetBSD: ptree.c,v 1.10 2012/10/06 22:15:09 matt Exp $");
#else
#include <stddef.h>
#include <stdint.h>
#include <limits.h>
#include <stdbool.h>
#include <string.h>
#ifdef PTDEBUG
#include <assert.h>
#define	KASSERT(e)	assert(e)
#else
#define	KASSERT(e)	do { } while (/*CONSTCOND*/ 0)
#endif
__RCSID("$NetBSD: ptree.c,v 1.10 2012/10/06 22:15:09 matt Exp $");
#endif /* _KERNEL || _STANDALONE */

#ifdef _LIBC
#include "namespace.h"
#endif

#ifdef PTTEST
#include "ptree.h"
#else
#include <sys/ptree.h>
#endif

/*
 * This is an implementation of a radix / PATRICIA tree.  As in a traditional
 * patricia tree, all the data is at the leaves of the tree.  An N-value
 * tree would have N leaves, N-1 branching nodes, and a root pointer.  Each
 * branching node would have left(0) and right(1) pointers that either point
 * to another branching node or a leaf node.  The root pointer would also
 * point to either the first branching node or a leaf node.  Leaf nodes
 * have no need for pointers.
 *
 * However, allocation for these branching nodes is problematic since the
 * allocation could fail.  This would cause insertions to fail for reasons
 * beyond the user's control.  So to prevent this, in this implementation
 * each node has two identities: its leaf identity and its branch identity.
 * Each is separate from the other.  Every branch is tagged as to whether
 * it points to a leaf or a branch.  This is not an attribute of the object
 * but of the pointer to the object.  The low bit of the pointer is used as
 * the tag to determine whether it points to a leaf or branch identity, with
 * branch identities having the low bit set.
 *
 * A node's branch identity has one rule: when traversing the tree from the
 * root to the node's leaf identity, one of the branches traversed will be via
 * the node's branch identity.  Of course, that has an exception: since to
 * store N leaves, you need N-1 branches.  That one node whose branch identity
 * isn't used is stored as "oddman"-out in the root.
 *
 * Branching nodes also has a bit offset and a bit length which determines
 * which branch slot is used.  The bit length can be zero resulting in a
 * one-way branch.  This happens in two special cases: the root and
 * interior mask nodes.
 *
 * To support longest match first lookups, when a mask node (one that only
 * match the first N bits) has children who first N bits match the mask nodes,
 * that mask node is converted from being a leaf node to being a one-way
 * branch-node.  The mask becomes fixed in position in the tree.  The mask
 * will always be the longest mask match for its descendants (unless they
 * traverse an even longer match).
 */

#define	NODETOITEM(pt, ptn)	\
	((void *)((uintptr_t)(ptn) - (pt)->pt_node_offset))
#define	NODETOKEY(pt, ptn)	\
	((void *)((uintptr_t)(ptn) - (pt)->pt_node_offset + pt->pt_key_offset))
#define	ITEMTONODE(pt, ptn)	\
	((pt_node_t *)((uintptr_t)(ptn) + (pt)->pt_node_offset))

bool ptree_check(const pt_tree_t *);
#if PTCHECK > 1
#define	PTREE_CHECK(pt)		ptree_check(pt)
#else
#define	PTREE_CHECK(pt)		do { } while (/*CONSTCOND*/ 0)
#endif

static inline bool
ptree_matchnode(const pt_tree_t *pt, const pt_node_t *target,
	const pt_node_t *ptn, pt_bitoff_t max_bitoff,
	pt_bitoff_t *bitoff_p, pt_slot_t *slots_p)
{
	return (*pt->pt_ops->ptto_matchnode)(NODETOKEY(pt, target),
	    (ptn != NULL ? NODETOKEY(pt, ptn) : NULL),
	    max_bitoff, bitoff_p, slots_p, pt->pt_context);
}

static inline pt_slot_t
ptree_testnode(const pt_tree_t *pt, const pt_node_t *target,
	const pt_node_t *ptn)
{
	const pt_bitlen_t bitlen = PTN_BRANCH_BITLEN(ptn);
	if (bitlen == 0)
		return PT_SLOT_ROOT;	/* mask or root, doesn't matter */
	return (*pt->pt_ops->ptto_testnode)(NODETOKEY(pt, target),
	    PTN_BRANCH_BITOFF(ptn), bitlen, pt->pt_context);
}

static inline bool
ptree_matchkey(const pt_tree_t *pt, const void *key,
	const pt_node_t *ptn, pt_bitoff_t bitoff, pt_bitlen_t bitlen)
{
	return (*pt->pt_ops->ptto_matchkey)(key, NODETOKEY(pt, ptn),
	    bitoff, bitlen, pt->pt_context);
}

static inline pt_slot_t
ptree_testkey(const pt_tree_t *pt, const void *key, const pt_node_t *ptn)
{
	const pt_bitlen_t bitlen = PTN_BRANCH_BITLEN(ptn);
	if (bitlen == 0)
		return PT_SLOT_ROOT;	/* mask or root, doesn't matter */
	return (*pt->pt_ops->ptto_testkey)(key, PTN_BRANCH_BITOFF(ptn),
	    PTN_BRANCH_BITLEN(ptn), pt->pt_context);
}

static inline void
ptree_set_position(uintptr_t node, pt_slot_t position)
{
	if (PT_LEAF_P(node))
		PTN_SET_LEAF_POSITION(PT_NODE(node), position);
	else
		PTN_SET_BRANCH_POSITION(PT_NODE(node), position);
}

void
ptree_init(pt_tree_t *pt, const pt_tree_ops_t *ops, void *context,
	size_t node_offset, size_t key_offset)
{
	memset(pt, 0, sizeof(*pt));
	pt->pt_node_offset = node_offset;
	pt->pt_key_offset = key_offset;
	pt->pt_context = context;
	pt->pt_ops = ops;
}

typedef struct {
	uintptr_t *id_insertp;
	pt_node_t *id_parent;
	uintptr_t id_node;
	pt_slot_t id_parent_slot;
	pt_bitoff_t id_bitoff;
	pt_slot_t id_slot;
} pt_insertdata_t;

typedef bool (*pt_insertfunc_t)(pt_tree_t *, pt_node_t *, pt_insertdata_t *);

/*
 * Move a branch identify from src to dst.  The leaves don't care since 
 * nothing for them has changed.
 */
/*ARGSUSED*/
static uintptr_t
ptree_move_branch(pt_tree_t * const pt, pt_node_t * const dst,
	const pt_node_t * const src)
{
	KASSERT(PTN_BRANCH_BITLEN(src) == 1);
	/* set branch bitlen and bitoff in one step.  */
	dst->ptn_branchdata = src->ptn_branchdata;
	PTN_SET_BRANCH_POSITION(dst, PTN_BRANCH_POSITION(src));
	PTN_COPY_BRANCH_SLOTS(dst, src);
	return PTN_BRANCH(dst);
}

#ifndef PTNOMASK
static inline uintptr_t *
ptree_find_branch(pt_tree_t * const pt, uintptr_t branch_node)
{
	pt_node_t * const branch = PT_NODE(branch_node);
	pt_node_t *parent;

	for (parent = &pt->pt_rootnode;;) {
		uintptr_t *nodep =
		    &PTN_BRANCH_SLOT(parent, ptree_testnode(pt, branch, parent));
		if (*nodep == branch_node)
			return nodep;
		if (PT_LEAF_P(*nodep))
			return NULL;
		parent = PT_NODE(*nodep);
	}
}

static bool
ptree_insert_leaf_after_mask(pt_tree_t * const pt, pt_node_t * const target,
	pt_insertdata_t * const id)
{
	const uintptr_t target_node = PTN_LEAF(target);
	const uintptr_t mask_node = id->id_node;
	pt_node_t * const mask = PT_NODE(mask_node);
	const pt_bitlen_t mask_len = PTN_MASK_BITLEN(mask);

	KASSERT(PT_LEAF_P(mask_node));
	KASSERT(PTN_LEAF_POSITION(mask) == id->id_parent_slot);
	KASSERT(mask_len <= id->id_bitoff);
	KASSERT(PTN_ISMASK_P(mask));
	KASSERT(!PTN_ISMASK_P(target) || mask_len < PTN_MASK_BITLEN(target));

	if (mask_node == PTN_BRANCH_ODDMAN_SLOT(&pt->pt_rootnode)) {
		KASSERT(id->id_parent != mask);
		/*
		 * Nice, mask was an oddman.  So just set the oddman to target.
		 */
		PTN_BRANCH_ODDMAN_SLOT(&pt->pt_rootnode) = target_node;
	} else {
		/*
		 * We need to find out who's pointing to mask's branch
		 * identity.  We know that between root and the leaf identity,
		 * we must traverse the node's branch identity.
		 */
		uintptr_t * const mask_nodep = ptree_find_branch(pt, PTN_BRANCH(mask));
		KASSERT(mask_nodep != NULL);
		KASSERT(*mask_nodep == PTN_BRANCH(mask));
		KASSERT(PTN_BRANCH_BITLEN(mask) == 1);

		/*
		 * Alas, mask was used as a branch.  Since the mask is becoming
		 * a one-way branch, we need make target take over mask's
		 * branching responsibilities.  Only then can we change it.
		 */
		*mask_nodep = ptree_move_branch(pt, target, mask);

		/*
		 * However, it's possible that mask's parent is itself.  If
		 * that's true, update the insert point to use target since it
		 * has taken over mask's branching duties.
		 */
		if (id->id_parent == mask)
			id->id_insertp = &PTN_BRANCH_SLOT(target,
			    id->id_parent_slot);
	}

	PTN_SET_BRANCH_BITLEN(mask, 0);
	PTN_SET_BRANCH_BITOFF(mask, mask_len);

	PTN_BRANCH_ROOT_SLOT(mask) = target_node;
	PTN_BRANCH_ODDMAN_SLOT(mask) = PT_NULL;
	PTN_SET_LEAF_POSITION(target, PT_SLOT_ROOT);
	PTN_SET_BRANCH_POSITION(mask, id->id_parent_slot);

	/*
	 * Now that everything is done, to make target visible we need to
	 * change mask from a leaf to a branch.
	 */
	*id->id_insertp = PTN_BRANCH(mask);
	PTREE_CHECK(pt);
	return true;
}

/*ARGSUSED*/
static bool
ptree_insert_mask_before_node(pt_tree_t * const pt, pt_node_t * const target,
	pt_insertdata_t * const id)
{
	const uintptr_t node = id->id_node;
	pt_node_t * const ptn = PT_NODE(node);
	const pt_slot_t mask_len = PTN_MASK_BITLEN(target);
	const pt_bitlen_t node_mask_len = PTN_MASK_BITLEN(ptn);

	KASSERT(PT_LEAF_P(node) || id->id_parent_slot == PTN_BRANCH_POSITION(ptn));
	KASSERT(PT_BRANCH_P(node) || id->id_parent_slot == PTN_LEAF_POSITION(ptn));
	KASSERT(PTN_ISMASK_P(target));

	/*
	 * If the node we are placing ourself in front is a mask with the
	 * same mask length as us, return failure.
	 */
	if (PTN_ISMASK_P(ptn) && node_mask_len == mask_len)
		return false;

	PTN_SET_BRANCH_BITLEN(target, 0);
	PTN_SET_BRANCH_BITOFF(target, mask_len);

	PTN_BRANCH_SLOT(target, PT_SLOT_ROOT) = node;
	*id->id_insertp = PTN_BRANCH(target);

	PTN_SET_BRANCH_POSITION(target, id->id_parent_slot);
	ptree_set_position(node, PT_SLOT_ROOT);

	PTREE_CHECK(pt);
	return true;
}
#endif /* !PTNOMASK */

/*ARGSUSED*/
static bool
ptree_insert_branch_at_node(pt_tree_t * const pt, pt_node_t * const target,
	pt_insertdata_t * const id)
{
	const uintptr_t target_node = PTN_LEAF(target);
	const uintptr_t node = id->id_node;
	const pt_slot_t other_slot = id->id_slot ^ PT_SLOT_OTHER;

	KASSERT(PT_BRANCH_P(node) || id->id_parent_slot == PTN_LEAF_POSITION(PT_NODE(node)));
	KASSERT(PT_LEAF_P(node) || id->id_parent_slot == PTN_BRANCH_POSITION(PT_NODE(node)));
	KASSERT((node == pt->pt_root) == (id->id_parent == &pt->pt_rootnode));
#ifndef PTNOMASK
	KASSERT(!PTN_ISMASK_P(target) || id->id_bitoff <= PTN_MASK_BITLEN(target));
#endif
	KASSERT(node == pt->pt_root || PTN_BRANCH_BITOFF(id->id_parent) + PTN_BRANCH_BITLEN(id->id_parent) <= id->id_bitoff);

	PTN_SET_BRANCH_BITOFF(target, id->id_bitoff);
	PTN_SET_BRANCH_BITLEN(target, 1);

	PTN_BRANCH_SLOT(target, id->id_slot) = target_node;
	PTN_BRANCH_SLOT(target, other_slot) = node;
	*id->id_insertp = PTN_BRANCH(target);

	PTN_SET_LEAF_POSITION(target, id->id_slot);
	ptree_set_position(node, other_slot);

	PTN_SET_BRANCH_POSITION(target, id->id_parent_slot);
	PTREE_CHECK(pt);
	return true;
}

static bool
ptree_insert_leaf(pt_tree_t * const pt, pt_node_t * const target,
	pt_insertdata_t * const id)
{
	const uintptr_t leaf_node = id->id_node;
	pt_node_t * const leaf = PT_NODE(leaf_node);
#ifdef PTNOMASK
	const bool inserting_mask = false;
	const bool at_mask = false;
#else
	const bool inserting_mask = PTN_ISMASK_P(target);
	const bool at_mask = PTN_ISMASK_P(leaf);
	const pt_bitlen_t leaf_masklen = PTN_MASK_BITLEN(leaf);
	const pt_bitlen_t target_masklen = PTN_MASK_BITLEN(target);
#endif
	pt_insertfunc_t insertfunc = ptree_insert_branch_at_node;
	bool matched;

	/*
	 * In all likelyhood we are going simply going to insert a branch
	 * where this leaf is which will point to the old and new leaves.
	 */
	KASSERT(PT_LEAF_P(leaf_node));
	KASSERT(PTN_LEAF_POSITION(leaf) == id->id_parent_slot);
	matched = ptree_matchnode(pt, target, leaf, UINT_MAX,
	    &id->id_bitoff, &id->id_slot);
	if (__predict_false(!inserting_mask)) {
		/*
		 * We aren't inserting a mask nor is the leaf a mask, which
		 * means we are trying to insert a duplicate leaf.  Can't do
		 * that.
		 */
		if (!at_mask && matched)
			return false;

#ifndef PTNOMASK
		/*
		 * We are at a mask and the leaf we are about to insert
		 * is at or beyond the mask, we need to convert the mask
		 * from a leaf to a one-way branch interior mask.
		 */
		if (at_mask && id->id_bitoff >= leaf_masklen)
			insertfunc = ptree_insert_leaf_after_mask;
#endif /* PTNOMASK */
	}
#ifndef PTNOMASK
	else {
		/*
		 * We are inserting a mask.
		 */
		if (matched) {
			/*
			 * If the leaf isn't a mask, we obviously have to
			 * insert the new mask before non-mask leaf.  If the
			 * leaf is a mask, and the new node has a LEQ mask
			 * length it too needs to inserted before leaf (*).
			 *
			 * In other cases, we place the new mask as leaf after
			 * leaf mask.  Which mask comes first will be a one-way
			 * branch interior mask node which has the other mask
			 * node as a child.
			 *
			 * (*) ptree_insert_mask_before_node can detect a
			 * duplicate mask and return failure if needed.
			 */
			if (!at_mask || target_masklen <= leaf_masklen)
				insertfunc = ptree_insert_mask_before_node;
			else
				insertfunc = ptree_insert_leaf_after_mask;
		} else if (at_mask && id->id_bitoff >= leaf_masklen) {
			/*
			 * If the new mask has a bit offset GEQ than the leaf's
			 * mask length, convert the left to a one-way branch
			 * interior mask and make that point to the new [leaf]
			 * mask.
			 */
			insertfunc = ptree_insert_leaf_after_mask;
		} else {
			/*
			 * The new mask has a bit offset less than the leaf's
			 * mask length or if the leaf isn't a mask at all, the
			 * new mask deserves to be its own leaf so we use the
			 * default insertfunc to do that.
			 */
		}
	}
#endif /* PTNOMASK */

	return (*insertfunc)(pt, target, id);
}

static bool
ptree_insert_node_common(pt_tree_t *pt, void *item)
{
	pt_node_t * const target = ITEMTONODE(pt, item);
#ifndef PTNOMASK
	const bool inserting_mask = PTN_ISMASK_P(target);
	const pt_bitlen_t target_masklen = PTN_MASK_BITLEN(target);
#endif
	pt_insertfunc_t insertfunc;
	pt_insertdata_t id;

	/*
	 * If this node already exists in the tree, return failure.
	 */
	if (target == PT_NODE(pt->pt_root))
		return false;

	/*
	 * We need a leaf so we can match against.  Until we get a leaf
	 * we having nothing to test against.
	 */
	if (__predict_false(PT_NULL_P(pt->pt_root))) {
		PTN_BRANCH_ROOT_SLOT(&pt->pt_rootnode) = PTN_LEAF(target);
		PTN_BRANCH_ODDMAN_SLOT(&pt->pt_rootnode) = PTN_LEAF(target);
		PTN_SET_LEAF_POSITION(target, PT_SLOT_ROOT);
		PTREE_CHECK(pt);
		return true;
	}

	id.id_bitoff = 0;
	id.id_parent = &pt->pt_rootnode;
	id.id_parent_slot = PT_SLOT_ROOT;
	id.id_insertp = &PTN_BRANCH_ROOT_SLOT(id.id_parent);
	for (;;) {
		pt_bitoff_t branch_bitoff;
		pt_node_t * const ptn = PT_NODE(*id.id_insertp);
		id.id_node = *id.id_insertp;

		/*
		 * If this node already exists in the tree, return failure.
		 */
		if (target == ptn)
			return false;

		/*
		 * If we hit a leaf, try to insert target at leaf.  We could
		 * have inlined ptree_insert_leaf here but that would have
		 * made this routine much harder to understand.  Trust the
		 * compiler to optimize this properly.
		 */
		if (PT_LEAF_P(id.id_node)) {
			KASSERT(PTN_LEAF_POSITION(ptn) == id.id_parent_slot);
			insertfunc = ptree_insert_leaf;
			break;
		}

		/*
		 * If we aren't a leaf, we must be a branch.  Make sure we are
		 * in the slot we think we are.
		 */
		KASSERT(PT_BRANCH_P(id.id_node));
		KASSERT(PTN_BRANCH_POSITION(ptn) == id.id_parent_slot);

		/*
		 * Where is this branch?
		 */
		branch_bitoff = PTN_BRANCH_BITOFF(ptn);

#ifndef PTNOMASK
		/*
		 * If this is a one-way mask node, its offset must equal
		 * its mask's bitlen.
		 */
		KASSERT(!(PTN_ISMASK_P(ptn) && PTN_BRANCH_BITLEN(ptn) == 0) || PTN_MASK_BITLEN(ptn) == branch_bitoff);

		/*
		 * If we are inserting a mask, and we know that at this point
		 * all bits before the current bit offset match both the target
		 * and the branch.  If the target's mask length is LEQ than
		 * this branch's bit offset, then this is where the mask needs
		 * to added to the tree.
		 */
		if (__predict_false(inserting_mask)
		    && (PTN_ISROOT_P(pt, id.id_parent)
			|| id.id_bitoff < target_masklen)
		    && target_masklen <= branch_bitoff) {
			/*
			 * We don't know about the bits (if any) between
			 * id.id_bitoff and the target's mask length match
			 * both the target and the branch.  If the target's
			 * mask length is greater than the current bit offset
			 * make sure the untested bits match both the target
			 * and the branch.
			 */
			if (target_masklen == id.id_bitoff
			    || ptree_matchnode(pt, target, ptn, target_masklen,
				    &id.id_bitoff, &id.id_slot)) {
				/*
				 * The bits matched, so insert the mask as a
				 * one-way branch.
				 */
				insertfunc = ptree_insert_mask_before_node;
				break;
			} else if (id.id_bitoff < branch_bitoff) {
				/*
				 * They didn't match, so create a normal branch
				 * because this mask needs to a be a new leaf.
				 */
				insertfunc = ptree_insert_branch_at_node;
				break;
			}
		}
#endif /* PTNOMASK */

		/*
		 * If we are skipping some bits, verify they match the node.
		 * If they don't match, it means we have a leaf to insert.
		 * Note that if we are advancing bit by bit, we'll skip
		 * doing matchnode and walk the tree bit by bit via testnode.
		 */
		if (id.id_bitoff < branch_bitoff
		    && !ptree_matchnode(pt, target, ptn, branch_bitoff,
					&id.id_bitoff, &id.id_slot)) {
			KASSERT(id.id_bitoff < branch_bitoff);
			insertfunc = ptree_insert_branch_at_node;
			break;
		}

		/*
		 * At this point, all bits before branch_bitoff are known
		 * to match the target.
		 */
		KASSERT(id.id_bitoff >= branch_bitoff);

		/*
		 * Decend the tree one level.
		 */
		id.id_parent = ptn;
		id.id_parent_slot = ptree_testnode(pt, target, id.id_parent);
		id.id_bitoff += PTN_BRANCH_BITLEN(id.id_parent);
		id.id_insertp = &PTN_BRANCH_SLOT(id.id_parent, id.id_parent_slot);
	}

	/*
	 * Do the actual insertion.
	 */
	return (*insertfunc)(pt, target, &id);
}

bool
ptree_insert_node(pt_tree_t *pt, void *item)
{
	pt_node_t * const target = ITEMTONODE(pt, item);

	memset(target, 0, sizeof(*target));
	return ptree_insert_node_common(pt, target);
}

#ifndef PTNOMASK
bool
ptree_insert_mask_node(pt_tree_t *pt, void *item, pt_bitlen_t mask_len)
{
	pt_node_t * const target = ITEMTONODE(pt, item);
	pt_bitoff_t bitoff = mask_len;
	pt_slot_t slot;

	memset(target, 0, sizeof(*target));
	KASSERT(mask_len == 0 || (~PT__MASK(PTN_MASK_BITLEN) & mask_len) == 0);
	/*
	 * Only the first <mask_len> bits can be non-zero.
	 * All other bits must be 0.
	 */
	if (!ptree_matchnode(pt, target, NULL, UINT_MAX, &bitoff, &slot))
		return false;
	PTN_SET_MASK_BITLEN(target, mask_len);
	PTN_MARK_MASK(target);
	return ptree_insert_node_common(pt, target);
}
#endif /* !PTNOMASH */

void *
ptree_find_filtered_node(pt_tree_t *pt, const void *key, pt_filter_t filter,
	void *filter_arg)
{
#ifndef PTNOMASK
	pt_node_t *mask = NULL;
#endif
	bool at_mask = false;
	pt_node_t *ptn, *parent;
	pt_bitoff_t bitoff;
	pt_slot_t parent_slot;

	if (PT_NULL_P(PTN_BRANCH_ROOT_SLOT(&pt->pt_rootnode)))
		return NULL;

	bitoff = 0;
	parent = &pt->pt_rootnode;
	parent_slot = PT_SLOT_ROOT;
	for (;;) {
		const uintptr_t node = PTN_BRANCH_SLOT(parent, parent_slot);
		const pt_slot_t branch_bitoff = PTN_BRANCH_BITOFF(PT_NODE(node));
		ptn = PT_NODE(node);

		if (PT_LEAF_P(node)) {
#ifndef PTNOMASK
			at_mask = PTN_ISMASK_P(ptn);
#endif
			break;
		}

		if (bitoff < branch_bitoff) {
			if (!ptree_matchkey(pt, key, ptn, bitoff, branch_bitoff - bitoff)) {
#ifndef PTNOMASK
				if (mask != NULL)
					return NODETOITEM(pt, mask);
#endif
				return NULL;
			}
			bitoff = branch_bitoff;
		}

#ifndef PTNOMASK
		if (PTN_ISMASK_P(ptn) && PTN_BRANCH_BITLEN(ptn) == 0
		    && (!filter
		        || (*filter)(filter_arg, NODETOITEM(pt, ptn),
				     PT_FILTER_MASK)))
			mask = ptn;
#endif

		parent = ptn;
		parent_slot = ptree_testkey(pt, key, parent);
		bitoff += PTN_BRANCH_BITLEN(parent);
	}

	KASSERT(PTN_ISROOT_P(pt, parent) || PTN_BRANCH_BITOFF(parent) + PTN_BRANCH_BITLEN(parent) == bitoff);
	if (!filter || (*filter)(filter_arg, NODETOITEM(pt, ptn), at_mask ? PT_FILTER_MASK : 0)) {
#ifndef PTNOMASK
		if (PTN_ISMASK_P(ptn)) {
			const pt_bitlen_t mask_len = PTN_MASK_BITLEN(ptn);
			if (bitoff == PTN_MASK_BITLEN(ptn))
				return NODETOITEM(pt, ptn);
			if (ptree_matchkey(pt, key, ptn, bitoff, mask_len - bitoff))
				return NODETOITEM(pt, ptn);
		} else
#endif /* !PTNOMASK */
		if (ptree_matchkey(pt, key, ptn, bitoff, UINT_MAX))
			return NODETOITEM(pt, ptn);
	}

#ifndef PTNOMASK
	/*
	 * By virtue of how the mask was placed in the tree,
	 * all nodes descended from it will match it.  But the bits
	 * before the mask still need to be checked and since the
	 * mask was a branch, that was done implicitly.
	 */
	if (mask != NULL) {
		KASSERT(ptree_matchkey(pt, key, mask, 0, PTN_MASK_BITLEN(mask)));
		return NODETOITEM(pt, mask);
	}
#endif /* !PTNOMASK */

	/*
	 * Nothing matched.
	 */
	return NULL;
}

void *
ptree_iterate(pt_tree_t *pt, const void *item, pt_direction_t direction)
{
	const pt_node_t * const target = ITEMTONODE(pt, item);
	uintptr_t node, next_node;

	if (direction != PT_ASCENDING && direction != PT_DESCENDING)
		return NULL;

	node = PTN_BRANCH_ROOT_SLOT(&pt->pt_rootnode);
	if (PT_NULL_P(node))
		return NULL;

	if (item == NULL) {
		pt_node_t * const ptn = PT_NODE(node);
		if (direction == PT_ASCENDING
		    && PTN_ISMASK_P(ptn) && PTN_BRANCH_BITLEN(ptn) == 0)
			return NODETOITEM(pt, ptn);
		next_node = node;
	} else {
#ifndef PTNOMASK
		uintptr_t mask_node = PT_NULL;
#endif /* !PTNOMASK */
		next_node = PT_NULL;
		while (!PT_LEAF_P(node)) { 
			pt_node_t * const ptn = PT_NODE(node);
			pt_slot_t slot;
#ifndef PTNOMASK
			if (PTN_ISMASK_P(ptn) && PTN_BRANCH_BITLEN(ptn) == 0) {
				if (ptn == target)
					break;
				if (direction == PT_DESCENDING) {
					mask_node = node;
					next_node = PT_NULL;
				}
			}
#endif /* !PTNOMASK */
			slot = ptree_testnode(pt, target, ptn);
			node = PTN_BRANCH_SLOT(ptn, slot);
			if (direction == PT_ASCENDING) {
				if (slot != (pt_slot_t)((1 << PTN_BRANCH_BITLEN(ptn)) - 1))
					next_node = PTN_BRANCH_SLOT(ptn, slot + 1);
			} else {
				if (slot > 0) {
#ifndef PTNOMASK
					mask_node = PT_NULL;
#endif /* !PTNOMASK */
					next_node = PTN_BRANCH_SLOT(ptn, slot - 1);
				}
			}
		}
		if (PT_NODE(node) != target)
			return NULL;
#ifndef PTNOMASK
		if (PT_BRANCH_P(node)) {
			pt_node_t *ptn = PT_NODE(node);
			KASSERT(PTN_ISMASK_P(PT_NODE(node)) && PTN_BRANCH_BITLEN(PT_NODE(node)) == 0);
			if (direction == PT_ASCENDING) {
				next_node = PTN_BRANCH_ROOT_SLOT(ptn);
				ptn = PT_NODE(next_node);
			}
		}
		/*
		 * When descending, if we countered a mask node then that's
		 * we want to return.
		 */
		if (direction == PT_DESCENDING && !PT_NULL_P(mask_node)) {
			KASSERT(PT_NULL_P(next_node));
			return NODETOITEM(pt, PT_NODE(mask_node));
		}
#endif /* !PTNOMASK */
	}

	node = next_node;
	if (PT_NULL_P(node))
		return NULL;

	while (!PT_LEAF_P(node)) {
		pt_node_t * const ptn = PT_NODE(node);
		pt_slot_t slot;
		if (direction == PT_ASCENDING) {
#ifndef PTNOMASK
			if (PT_BRANCH_P(node)
			    && PTN_ISMASK_P(ptn)
			    && PTN_BRANCH_BITLEN(ptn) == 0)
				return NODETOITEM(pt, ptn);
#endif /* !PTNOMASK */
			slot = PT_SLOT_LEFT;
		} else {
			slot = (1 << PTN_BRANCH_BITLEN(ptn)) - 1;
		}
		node = PTN_BRANCH_SLOT(ptn, slot);
	}
	return NODETOITEM(pt, PT_NODE(node));
}

void
ptree_remove_node(pt_tree_t *pt, void *item)
{
	pt_node_t * const target = ITEMTONODE(pt, item);
	const pt_slot_t leaf_slot = PTN_LEAF_POSITION(target);
	const pt_slot_t branch_slot = PTN_BRANCH_POSITION(target);
	pt_node_t *ptn, *parent;
	uintptr_t node;
	uintptr_t *removep;
	uintptr_t *nodep;
	pt_bitoff_t bitoff;
	pt_slot_t parent_slot;
#ifndef PTNOMASK
	bool at_mask;
#endif

	if (PT_NULL_P(PTN_BRANCH_ROOT_SLOT(&pt->pt_rootnode))) {
		KASSERT(!PT_NULL_P(PTN_BRANCH_ROOT_SLOT(&pt->pt_rootnode)));
		return;
	}

	bitoff = 0;
	removep = NULL;
	nodep = NULL;
	parent = &pt->pt_rootnode;
	parent_slot = PT_SLOT_ROOT;
	for (;;) {
		node = PTN_BRANCH_SLOT(parent, parent_slot);
		ptn = PT_NODE(node);
#ifndef PTNOMASK
		at_mask = PTN_ISMASK_P(ptn);
#endif

		if (PT_LEAF_P(node))
			break;

		/*
		 * If we are at the target, then we are looking at its branch
		 * identity.  We need to remember who's pointing at it so we
		 * stop them from doing that.
		 */
		if (__predict_false(ptn == target)) {
			KASSERT(nodep == NULL);
#ifndef PTNOMASK
			/*
			 * Interior mask nodes are trivial to get rid of.
			 */
			if (at_mask && PTN_BRANCH_BITLEN(ptn) == 0) {
				PTN_BRANCH_SLOT(parent, parent_slot) =
				    PTN_BRANCH_ROOT_SLOT(ptn);
				KASSERT(PT_NULL_P(PTN_BRANCH_ODDMAN_SLOT(ptn)));
				PTREE_CHECK(pt);
				return;
			}
#endif /* !PTNOMASK */
			nodep = &PTN_BRANCH_SLOT(parent, parent_slot);
			KASSERT(*nodep == PTN_BRANCH(target));
		}
		/*
		 * We need also need to know who's pointing at our parent.
		 * After we remove ourselves from our parent, he'll only
		 * have one child and that's unacceptable.  So we replace
		 * the pointer to the parent with our abadoned sibling.
		 */
		removep = &PTN_BRANCH_SLOT(parent, parent_slot);

		/*
		 * Descend into the tree.
		 */
		parent = ptn;
		parent_slot = ptree_testnode(pt, target, parent);
		bitoff += PTN_BRANCH_BITLEN(parent);
	}

	/*
	 * We better have found that the leaf we are looking for is target.
	 */
	if (target != ptn) {
		KASSERT(target == ptn);
		return;
	}

	/*
	 * If we didn't encounter target as branch, then target must be the
	 * oddman-out.
	 */
	if (nodep == NULL) {
		KASSERT(PTN_BRANCH_ODDMAN_SLOT(&pt->pt_rootnode) == PTN_LEAF(target));
		KASSERT(nodep == NULL);
		nodep = &PTN_BRANCH_ODDMAN_SLOT(&pt->pt_rootnode);
	}

	KASSERT((removep == NULL) == (parent == &pt->pt_rootnode));

	/*
	 * We have to special remove the last leaf from the root since
	 * the only time the tree can a PT_NULL node is when it's empty.
	 */
	if (__predict_false(PTN_ISROOT_P(pt, parent))) {
		KASSERT(removep == NULL);
		KASSERT(parent == &pt->pt_rootnode);
		KASSERT(nodep == &PTN_BRANCH_ODDMAN_SLOT(&pt->pt_rootnode));
		KASSERT(*nodep == PTN_LEAF(target));
		PTN_BRANCH_ROOT_SLOT(&pt->pt_rootnode) = PT_NULL;
		PTN_BRANCH_ODDMAN_SLOT(&pt->pt_rootnode) = PT_NULL;
		return;
	}

	KASSERT((parent == target) == (removep == nodep));
	if (PTN_BRANCH(parent) == PTN_BRANCH_SLOT(target, PTN_BRANCH_POSITION(parent))) {
		/*
		 * The pointer to the parent actually lives in the target's
		 * branch identity.  We can't just move the target's branch
		 * identity since that would result in the parent pointing
		 * to its own branch identity and that's fobidden.
		 */
		const pt_slot_t slot = PTN_BRANCH_POSITION(parent);
		const pt_slot_t other_slot = slot ^ PT_SLOT_OTHER;
		const pt_bitlen_t parent_bitlen = PTN_BRANCH_BITLEN(parent);

		KASSERT(PTN_BRANCH_BITOFF(target) < PTN_BRANCH_BITOFF(parent));

		/*
		 * This gets so confusing.  The target's branch identity
		 * points to the branch identity of the parent of the target's
		 * leaf identity:
		 *
		 * 	TB = { X, PB = { TL, Y } }
		 *   or TB = { X, PB = { TL } }
		 *
		 * So we can't move the target's branch identity to the parent
		 * because that would corrupt the tree.
		 */
		if (__predict_true(parent_bitlen > 0)) {
			/*
			 * The parent is a two-way branch.  We have to have
			 * do to this chang in two steps to keep internally
			 * consistent.  First step is to copy our sibling from
			 * our parent to where we are pointing to parent's
			 * branch identiy.  This remove all references to his
			 * branch identity from the tree.  We then simply make
			 * the parent assume the target's branching duties.
			 *
			 *   TB = { X, PB = { Y, TL } } --> PB = { X, Y }.
			 *   TB = { X, PB = { TL, Y } } --> PB = { X, Y }.
			 *   TB = { PB = { Y, TL }, X } --> PB = { Y, X }.
			 *   TB = { PB = { TL, Y }, X } --> PB = { Y, X }.
			 */
			PTN_BRANCH_SLOT(target, slot) =
			    PTN_BRANCH_SLOT(parent, parent_slot ^ PT_SLOT_OTHER);
			*nodep = ptree_move_branch(pt, parent, target);
			PTREE_CHECK(pt);
			return;
		} else {
			/*
			 * If parent was a one-way branch, it must have been
			 * mask which pointed to a single leaf which we are
			 * removing.  This means we have to convert the
			 * parent back to a leaf node.  So in the same
			 * position that target pointed to parent, we place
			 * leaf pointer to parent.  In the other position,
			 * we just put the other node from target.
			 *
			 *   TB = { X, PB = { TL } } --> PB = { X, PL }
			 */
			KASSERT(PTN_ISMASK_P(parent));
			KASSERT(slot == ptree_testnode(pt, parent, target));
			PTN_BRANCH_SLOT(parent, slot) = PTN_LEAF(parent);
			PTN_BRANCH_SLOT(parent, other_slot) =
			   PTN_BRANCH_SLOT(target, other_slot);
			PTN_SET_LEAF_POSITION(parent,slot);
			PTN_SET_BRANCH_BITLEN(parent, 1);
		}
		PTN_SET_BRANCH_BITOFF(parent, PTN_BRANCH_BITOFF(target));
		PTN_SET_BRANCH_POSITION(parent, PTN_BRANCH_POSITION(target));

		*nodep = PTN_BRANCH(parent);
		PTREE_CHECK(pt);
		return;
	}

#ifndef PTNOMASK
	if (__predict_false(PTN_BRANCH_BITLEN(parent) == 0)) {
		/*
		 * Parent was a one-way branch which is changing back to a leaf.
		 * Since parent is no longer a one-way branch, it can take over
		 * target's branching duties.
		 *
		 *  GB = { PB = { TL } }	--> GB = { PL }
		 *  TB = { X, Y }		--> PB = { X, Y }
		 */
		KASSERT(PTN_ISMASK_P(parent));
		KASSERT(parent != target);
		*removep = PTN_LEAF(parent);
	} else
#endif /* !PTNOMASK */
	{
		/*
		 * Now we are the normal removal case.  Since after the
		 * target's leaf identity is removed from the its parent,
		 * that parent will only have one decendent.  So we can
		 * just as easily replace the node that has the parent's
		 * branch identity with the surviving node.  This freeing
		 * parent from its branching duties which means it can
		 * take over target's branching duties.
		 *
		 *  GB = { PB = { X, TL } }	--> GB = { X }
		 *  TB = { V, W }		--> PB = { V, W }
		 */
		const pt_slot_t other_slot = parent_slot ^ PT_SLOT_OTHER;
		uintptr_t other_node = PTN_BRANCH_SLOT(parent, other_slot);
		const pt_slot_t target_slot = (parent == target ? branch_slot : leaf_slot);

		*removep = other_node;
		
		ptree_set_position(other_node, target_slot);

		/*
		 * If target's branch identity contained its leaf identity, we
		 * have nothing left to do.  We've already moved 'X' so there
		 * is no longer anything in the target's branch identiy that 
		 * has to be preserved.
		 */
		if (parent == target) {
			/*
			 *  GB = { TB = { X, TL } }	--> GB = { X }
			 *  TB = { X, TL }		--> don't care
			 */
			PTREE_CHECK(pt);
			return;
		}
	}

	/*
	 * If target wasn't used as a branch, then it must have been the
	 * oddman-out of the tree (the one node that doesn't have a branch
	 * identity).  This makes parent the new oddman-out.
	 */
	if (*nodep == PTN_LEAF(target)) {
		KASSERT(nodep == &PTN_BRANCH_ODDMAN_SLOT(&pt->pt_rootnode));
		PTN_BRANCH_ODDMAN_SLOT(&pt->pt_rootnode) = PTN_LEAF(parent);
		PTREE_CHECK(pt);
		return;
	}

	/*
	 * Finally move the target's branching duties to the parent.
	 */
	KASSERT(PTN_BRANCH_BITOFF(parent) > PTN_BRANCH_BITOFF(target));
	*nodep = ptree_move_branch(pt, parent, target);
	PTREE_CHECK(pt);
}

#ifdef PTCHECK
static const pt_node_t *
ptree_check_find_node2(const pt_tree_t *pt, const pt_node_t *parent,
	uintptr_t target)
{
	const pt_bitlen_t slots = 1 << PTN_BRANCH_BITLEN(parent);
	pt_slot_t slot;

	for (slot = 0; slot < slots; slot++) {
		const uintptr_t node = PTN_BRANCH_SLOT(parent, slot);
		if (PTN_BRANCH_SLOT(parent, slot) == node)
			return parent;
	}
	for (slot = 0; slot < slots; slot++) {
		const uintptr_t node = PTN_BRANCH_SLOT(parent, slot);
		const pt_node_t *branch;
		if (!PT_BRANCH_P(node))
			continue;
		branch = ptree_check_find_node2(pt, PT_NODE(node), target);
		if (branch != NULL)
			return branch;
	}

	return NULL;
}

static bool
ptree_check_leaf(const pt_tree_t *pt, const pt_node_t *parent,
	const pt_node_t *ptn)
{
	const pt_bitoff_t leaf_position = PTN_LEAF_POSITION(ptn);
	const pt_bitlen_t bitlen = PTN_BRANCH_BITLEN(ptn);
	const pt_bitlen_t mask_len = PTN_MASK_BITLEN(ptn);
	const uintptr_t leaf_node = PTN_LEAF(ptn);
	const bool is_parent_root = (parent == &pt->pt_rootnode);
	const bool is_mask = PTN_ISMASK_P(ptn);
	bool ok = true;

	if (is_parent_root) {
		ok = ok && PTN_BRANCH_ODDMAN_SLOT(parent) == leaf_node;
		KASSERT(ok);
		return ok;
	}

	if (is_mask && PTN_ISMASK_P(parent) && PTN_BRANCH_BITLEN(parent) == 0) {
		ok = ok && PTN_MASK_BITLEN(parent) < mask_len;
		KASSERT(ok);
		ok = ok && PTN_BRANCH_BITOFF(parent) < mask_len;
		KASSERT(ok);
	}
	ok = ok && PTN_BRANCH_SLOT(parent, leaf_position) == leaf_node;
	KASSERT(ok);
	ok = ok && leaf_position == ptree_testnode(pt, ptn, parent);
	KASSERT(ok);
	if (PTN_BRANCH_ODDMAN_SLOT(&pt->pt_rootnode) != leaf_node) {
		ok = ok && bitlen > 0;
		KASSERT(ok);
		ok = ok && ptn == ptree_check_find_node2(pt, ptn, PTN_LEAF(ptn));
		KASSERT(ok);
	}
	return ok;
}

static bool
ptree_check_branch(const pt_tree_t *pt, const pt_node_t *parent,
	const pt_node_t *ptn)
{
	const bool is_parent_root = (parent == &pt->pt_rootnode);
	const pt_slot_t branch_slot = PTN_BRANCH_POSITION(ptn);
	const pt_bitoff_t bitoff = PTN_BRANCH_BITOFF(ptn);
	const pt_bitoff_t bitlen = PTN_BRANCH_BITLEN(ptn);
	const pt_bitoff_t parent_bitoff = PTN_BRANCH_BITOFF(parent);
	const pt_bitoff_t parent_bitlen = PTN_BRANCH_BITLEN(parent);
	const bool is_parent_mask = PTN_ISMASK_P(parent) && parent_bitlen == 0;
	const bool is_mask = PTN_ISMASK_P(ptn) && bitlen == 0;
	const pt_bitoff_t parent_mask_len = PTN_MASK_BITLEN(parent);
	const pt_bitoff_t mask_len = PTN_MASK_BITLEN(ptn);
	const pt_bitlen_t slots = 1 << bitlen;
	pt_slot_t slot;
	bool ok = true;

	ok = ok && PTN_BRANCH_SLOT(parent, branch_slot) == PTN_BRANCH(ptn);
	KASSERT(ok);
	ok = ok && branch_slot == ptree_testnode(pt, ptn, parent);
	KASSERT(ok);

	if (is_mask) {
		ok = ok && bitoff == mask_len;
		KASSERT(ok);
		if (is_parent_mask) {
			ok = ok && parent_mask_len < mask_len;
			KASSERT(ok);
			ok = ok && parent_bitoff < bitoff;
			KASSERT(ok);
		}
	} else {
		if (is_parent_mask) {
			ok = ok && parent_bitoff <= bitoff;
		} else if (!is_parent_root) {
			ok = ok && parent_bitoff < bitoff;
		}
		KASSERT(ok);
	}

	for (slot = 0; slot < slots; slot++) {
		const uintptr_t node = PTN_BRANCH_SLOT(ptn, slot);
		pt_bitoff_t tmp_bitoff = 0;
		pt_slot_t tmp_slot;
		ok = ok && node != PTN_BRANCH(ptn);
		KASSERT(ok);
		if (bitlen > 0) {
			ok = ok && ptree_matchnode(pt, PT_NODE(node), ptn, bitoff, &tmp_bitoff, &tmp_slot);
			KASSERT(ok);
			tmp_slot = ptree_testnode(pt, PT_NODE(node), ptn);
			ok = ok && slot == tmp_slot;
			KASSERT(ok);
		}
		if (PT_LEAF_P(node))
			ok = ok && ptree_check_leaf(pt, ptn, PT_NODE(node));
		else
			ok = ok && ptree_check_branch(pt, ptn, PT_NODE(node));
	}

	return ok;
}
#endif /* PTCHECK */

/*ARGSUSED*/
bool
ptree_check(const pt_tree_t *pt)
{
	bool ok = true;
#ifdef PTCHECK
	const pt_node_t * const parent = &pt->pt_rootnode;
	const uintptr_t node = pt->pt_root;
	const pt_node_t * const ptn = PT_NODE(node);

	ok = ok && PTN_BRANCH_BITOFF(parent) == 0;
	ok = ok && !PTN_ISMASK_P(parent);

	if (PT_NULL_P(node))
		return ok;

	if (PT_LEAF_P(node))
		ok = ok && ptree_check_leaf(pt, parent, ptn);
	else
		ok = ok && ptree_check_branch(pt, parent, ptn);
#endif
	return ok;
}

bool
ptree_mask_node_p(pt_tree_t *pt, const void *item, pt_bitlen_t *lenp)
{
	const pt_node_t * const mask = ITEMTONODE(pt, item);

	if (!PTN_ISMASK_P(mask))
		return false;

	if (lenp != NULL)
		*lenp = PTN_MASK_BITLEN(mask);

	return true;
}
