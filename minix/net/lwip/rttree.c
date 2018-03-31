/* LWIP service - rttree.c - generic routing tree data structure */
/*
 * This module implements the Net/3 binary radix (Patricia) tree as described
 * in TCP/IP Illustrated Vol.2, with a few important changes.  First and
 * foremost, we make the assumption that all address masks are "normal", i.e.,
 * they can be expressed in terms of a "prefix length" or "bit count", meaning
 * that the first so many bits of the mask are set and the remaining bits are
 * all clear.  Based on this assumption, we store routing entries not just in
 * leaf nodes, but rather in a node at the bit count of the routing entry's
 * mask; this node may then also have children.  As a result, instead of "leaf"
 * and "internal" nodes, this module instead uses "data" and "link" nodes:
 *
 * - Data nodes are nodes with an associated routing entry.  The data node
 *   structure is always the first field of its corresponding routing entry
 *   structure.  Data nodes may have zero, one, or two children.  Its children
 *   are always a refinement of the address mask in the routing entry.
 * - Link nodes are nodes with no associated routing entry.  They always have
 *   exactly two children.  As with BSD's "internal" nodes: since the tree
 *   needs no more than one link node per routing entry, each routing entry
 *   structure contains a link node, which may be used anywhere in the tree.
 *
 * The result of this approach is that we do not use a linked list for each
 * leaf, since entries with the same address and different masks are not stored
 * as part of the same leaf node.  There is however still one case where a
 * linked list would be necessary: the coexistence of a full-mask network entry
 * and a host entry (net/32 vs host for IPv4, net/128 vs host for IPv6).  Since
 * this tree implementation is not used for ARP/ND6 (host) entries, the need to
 * support that case is not as high, and so it is currently not supported.  It
 * can be added later if needed.  In that case, the prototype of only
 * rttree_find_exact() will have to be changed, since rttree_add() already
 * supports the difference by passing a full mask vs passing no mask at all.
 *
 * There are other differences with the BSD implementation, and certainly also
 * more opportunities for improving performance.  For now, the implementation
 * should be good enough for its intended purpose.
 */

#include "lwip.h"
#include "rttree.h"

#define RTTREE_BITS_TO_BYTE(bits)	((bits) >> 3)
#define RTTREE_BITS_TO_SHIFT(bits)	(7 - ((bits) & 7))
#define RTTREE_BITS_TO_BYTES(bits)	(RTTREE_BITS_TO_BYTE((bits) + 7))

/*
 * The given node is being added to the given routing tree, and just had its
 * bit count assigned.  Precompute any additional fields used for fast address
 * access on the node.
 */
static void
rttree_precompute(struct rttree * tree __unused, struct rttree_node * node)
{

	node->rtn_byte = RTTREE_BITS_TO_BYTE(node->rtn_bits);
	node->rtn_shift = RTTREE_BITS_TO_SHIFT(node->rtn_bits);
}

/*
 * For an operation on the routing tree 'tree', test whether the bit 'bit' is
 * set or clear in 'addr'.  Return 1 if the address has the bit set, 0 if it
 * does not.
 */
static unsigned int
rttree_test(const struct rttree * tree __unused, const void * addr,
	unsigned int bit)
{
	unsigned int byte, shift;

	byte = RTTREE_BITS_TO_BYTE(bit);
	shift = RTTREE_BITS_TO_SHIFT(bit);

	return (((const uint8_t *)addr)[byte] >> shift) & 1;
}

/*
 * For an operation on the routing tree 'tree', test whether a particular bit
 * as identified by the routing node 'node' is set or clear in 'address',
 * effectively computing the side (left or right) to take when descending down
 * the tree.  Return 1 if the address has the bit set, 0 if it does not.
 */
static inline unsigned int
rttree_side(const struct rttree * tree, const struct rttree_node * node,
	const void * addr)
{

	return (((const uint8_t *)addr)[node->rtn_byte] >>
	    node->rtn_shift) & 1;
}

/*
 * Check for the routing tree 'tree' whether the routing entry 'entry' matches
 * the address 'addr' exactly.  Return TRUE or FALSE depending on the outcome.
 * This function must be called only on entries that have already been
 * determined to span the full bit width.
 */
static inline int
rttree_equals(const struct rttree * tree, const struct rttree_entry * entry,
	const void * addr)
{
	unsigned int bits;

	bits = tree->rtt_bits;

	assert(bits == entry->rte_data.rtn_bits);

	return !memcmp(entry->rte_addr, addr, RTTREE_BITS_TO_BYTE(bits));
}

/*
 * Check for the routing tree 'tree' whether the routing entry 'entry' matches
 * the address 'addr'.  Return TRUE if the address is matched by the entry's
 * address and mask, or FALSE if not.
 */
static inline int
rttree_match(const struct rttree * tree, const struct rttree_entry * entry,
	const void * addr)
{
	const uint8_t *aptr, *aptr2, *mptr;
	unsigned int bits, bytes;

	if ((bits = entry->rte_data.rtn_bits) == 0)
		return TRUE;

	if ((mptr = (const uint8_t *)entry->rte_mask) == NULL)
		return rttree_equals(tree, entry, addr);

	aptr = (const uint8_t *)addr;
	aptr2 = (const uint8_t *)entry->rte_addr;

	for (bytes = RTTREE_BITS_TO_BYTES(bits); bytes > 0; bytes--) {
		if ((*aptr & *mptr) != *aptr2)
			return FALSE;

		aptr++;
		aptr2++;
		mptr++;
	}

	return TRUE;
}

/*
 * Find the first bit that differs between the two given addresses.  Return the
 * bit number if found, or the full bit width if the addresses are equal.
 */
static unsigned int
rttree_diff(const struct rttree * tree, const void * addr, const void * addr2)
{
	const uint8_t *aptr, *aptr2;
	unsigned int bit, i;
	uint8_t b;

	aptr = (const uint8_t *)addr;
	aptr2 = (const uint8_t *)addr2;

	for (bit = 0; bit < tree->rtt_bits; bit += NBBY, aptr++, aptr2++) {
		if ((b = *aptr ^ *aptr2) != 0) {
			for (i = 0; i < NBBY; i++)
				if (b & (1 << (NBBY - i - 1)))
					break;
			return bit + i;
		}
	}

	return bit;
}

/*
 * Add a link node to the free list of the given routing tree, marking it as
 * free in the process.
 */
static void
rttree_add_free(struct rttree * tree, struct rttree_node * node)
{

	node->rtn_child[0] = NULL;
	if ((node->rtn_child[1] = tree->rtt_free) != NULL)
		node->rtn_child[1]->rtn_child[0] = node;
	tree->rtt_free = node;
	node->rtn_parent = NULL;
	node->rtn_type = RTNT_FREE;
}

/*
 * Remove the given free link node from the free list.  The caller must already
 * have verified that the node is on the free list, and has to change the node
 * type as appropriate afterward.
 */
static void
rttree_del_free(struct rttree * tree, struct rttree_node * node)
{

	assert(node->rtn_type == RTNT_FREE);

	if (node->rtn_child[0] != NULL)
		node->rtn_child[0]->rtn_child[1] = node->rtn_child[1];
	else
		tree->rtt_free = node->rtn_child[1];
	if (node->rtn_child[1] != NULL)
		node->rtn_child[1]->rtn_child[0] = node->rtn_child[0];
}

/*
 * Obtain, remove, and return a free link node from the free list.  This
 * function must be called only when it is already known that the free list is
 * not empty.  The caller has to change the node type as appropriate afterward.
 */
static struct rttree_node *
rttree_get_free(struct rttree * tree)
{
	struct rttree_node * node;

	node = tree->rtt_free;
	assert(node != NULL);
	assert(node->rtn_type == RTNT_FREE);

	rttree_del_free(tree, node);

	return node;
}

/*
 * Initialize the given routing tree, with the given address bit width.
 */
void
rttree_init(struct rttree * tree, unsigned int bits)
{

	tree->rtt_root = NULL;
	tree->rtt_free = NULL;
	tree->rtt_bits = bits;
}

/*
 * Look up the most narrow routing tree entry that matches the given address.
 * Return the entry on success, or NULL if no matching entry is found.
 */
struct rttree_entry *
rttree_lookup_match(struct rttree * tree, const void * addr)
{
	struct rttree_entry *entry, *best;
	struct rttree_node *node;
	unsigned int side;

	/*
	 * The current implementation is "forward-tracking", testing all
	 * potentially matching entries while descending into the tree and
	 * remembering the "best" (narrowest matching) entry.  The assumption
	 * here is that most lookups will end up returning the default route or
	 * another broad route, and thus quickly fail a narrower match and bail
	 * out early.  This assumption is in part motivated by the fact that
	 * our routing trees do not store link-layer (ARP/ND6) entries.  If
	 * desired, the implementation can easily be rewritten to do
	 * backtracking instead.
	 */
	best = NULL;

	for (node = tree->rtt_root; node != NULL;
	    node = node->rtn_child[side]) {
		if (node->rtn_type == RTNT_DATA) {
			entry = (struct rttree_entry *)node;

			if (!rttree_match(tree, entry, addr))
				break;

			best = entry;
		}

		side = rttree_side(tree, node, addr);
	}

	return best;
}

/*
 * Look up a routing entry that is an exact match for the given (full) address.
 * Return the entry if it was found, or NULL otherwise.
 */
struct rttree_entry *
rttree_lookup_host(struct rttree * tree, const void * addr)
{
	struct rttree_entry *entry;
	struct rttree_node *node;
	unsigned int side;

	for (node = tree->rtt_root; node != NULL;
	    node = node->rtn_child[side]) {
		if (node->rtn_type == RTNT_DATA &&
		    node->rtn_bits == tree->rtt_bits) {
			entry = (struct rttree_entry *)node;

			if (rttree_equals(tree, entry, addr))
				return entry;

			break;
		}

		side = rttree_side(tree, node, addr);
	}

	return NULL;
}

/*
 * Look up a routing entry that is an exact match for the given address and
 * prefix length.  Return the entry if found, or NULL otherwise.
 */
struct rttree_entry *
rttree_lookup_exact(struct rttree * tree, const void * addr,
	unsigned int prefix)
{
	struct rttree_entry *entry;
	struct rttree_node *node;
	unsigned int side;

	for (node = tree->rtt_root; node != NULL && node->rtn_bits <= prefix;
	    node = node->rtn_child[side]) {
		if (node->rtn_type == RTNT_DATA) {
			entry = (struct rttree_entry *)node;

			if (!rttree_match(tree, entry, addr))
				return NULL;

			if (node->rtn_bits == prefix)
				return entry;
		}

		side = rttree_side(tree, node, addr);
	}

	return NULL;
}

/*
 * Enumerate entries in the routing tree.  If 'last' is NULL, return the first
 * entry.  Otherwise, return the next entry starting from 'last'.  In both
 * cases, if no (more) entries are present in the tree, return NULL.  The order
 * of the returned entries is stable across tree modifications and the function
 * may be called multiple times on the same entry.  More specifically, it is
 * safe to continue enumeration from a previous entry after deleting its
 * successor from the tree.
 */
struct rttree_entry *
rttree_enum(struct rttree * tree, struct rttree_entry * last)
{
	struct rttree_node *node, *parent;

	/*
	 * For the first query, we may have to return the tree root right away.
	 * For subsequent queries, we have to move ahead by at least one node.
	 */
	if (last == NULL) {
		if ((node = tree->rtt_root) == NULL)
			return NULL;

		if (node->rtn_type == RTNT_DATA)
			return (struct rttree_entry *)node;
	} else
		node = &last->rte_data;

	/* A basic iterative pre-order binary-tree depth-first search. */
	do {
		assert(node != NULL);

		/* Can we descend further, either left or right? */
		if (node->rtn_child[0] != NULL)
			node = node->rtn_child[0];
		else if (node->rtn_child[1] != NULL)
			node = node->rtn_child[1];
		else {
			/*
			 * No.  Go back up the tree, until we can go right
			 * where we went left before.. or run out of tree.
			 */
			for (;; node = parent) {
				if ((parent = node->rtn_parent) == NULL)
					return NULL;

				if (parent->rtn_child[0] == node &&
				    parent->rtn_child[1] != NULL) {
					node = parent->rtn_child[1];

					break;
				}
			}
		}

		/* Skip link nodes. */
	} while (node->rtn_type != RTNT_DATA);

	return (struct rttree_entry *)node;
}

/*
 * Set the node 'node' to be part of tree 'tree', with type 'type' (either
 * RTNT_DATA or RTNT_LINK) and a bit count of 'prefix'.  The node is set to be
 * a child of 'parent' on side 'side', unless 'parent' is NULL in which case
 * the node is set to be the topmost node in the tree (and 'side' is ignored).
 * The node's children are set to 'left' and 'right'; for each, if not NULL,
 * its parent is set to 'node'.
 */
static void
rttree_set(struct rttree * tree, struct rttree_node * node, int type,
	unsigned int prefix, struct rttree_node * parent, int side,
	struct rttree_node * left, struct rttree_node * right)
{

	assert(type == RTNT_DATA || type == RTNT_LINK);
	assert(prefix <= tree->rtt_bits);
	assert(side == 0 || side == 1);

	node->rtn_type = type;
	node->rtn_bits = prefix;

	/* With rtn_bits assigned, precompute any derived fields. */
	rttree_precompute(tree, node);

	if ((node->rtn_parent = parent) != NULL)
		parent->rtn_child[side] = node;
	else
		tree->rtt_root = node;

	if ((node->rtn_child[0] = left) != NULL)
		left->rtn_parent = node;
	if ((node->rtn_child[1] = right) != NULL)
		right->rtn_parent = node;
}

/*
 * In the routing tree 'tree', replace old node 'onode' with new node 'node',
 * setting the type of the latter to 'type'.  The tree is updated accordingly,
 * but it is left up to the caller to deal with the old node as appropriate.
 */
static void
rttree_replace(struct rttree * tree, struct rttree_node * onode,
	struct rttree_node * node, int type)
{
	struct rttree_node *parent;
	unsigned int side;

	/*
	 * Replacing one data node with another data node is not something that
	 * is currently being done, even if it would work.
	 */
	assert(onode->rtn_type != RTNT_DATA || node->rtn_type != RTNT_DATA);
	assert(onode->rtn_child[0] != NULL);
	assert(onode->rtn_child[1] != NULL);

	parent = onode->rtn_parent;

	side = (parent != NULL && parent->rtn_child[1] == onode);

	rttree_set(tree, node, type, onode->rtn_bits, parent, side,
	    onode->rtn_child[0], onode->rtn_child[1]);
}

/*
 * Add a new routing entry 'entry' to the routing tree 'tree'.  The entry
 * object will be initialized as a result.  The address to add is given as
 * 'addr', and the address mask as 'mask'.  Both those pointers must be point
 * to memory that is as long-lived as the routing entry; this is typically
 * accomplished by storing them in a larger object that embeds 'entry'.
 * However, 'mask' may be NULL, signifying a host type entry with an implied
 * full mask.  If not NULL, the given mask must be normalized, i.e., it must
 * consist of a run of zero or more 1-bits followed by a remainder of only
 * 0-bits.  The number of 1-bits must also be given as a bit count 'prefix',
 * even if 'mask' is NULL.  The address must be normalized to its mask: no bits
 * starting from bit 'prefix' must be set in 'addr'.  Return OK if adding the
 * routing entry succeeded, or EEXIST if an entry already exists for the
 * combination of that address and mask.  If the caller has already verified
 * with rttree_lookup_exact() that no such entry exists, the call will succeed.
 */
int
rttree_add(struct rttree * tree, struct rttree_entry * entry,
	const void * addr, const void * mask, unsigned int prefix)
{
	struct rttree_node *node, *parent, *link;
	struct rttree_entry *other_entry;
	unsigned int bit = 0 /*gcc*/, side, side2;
	int match;

	assert(mask != NULL || prefix == tree->rtt_bits);

	/*
	 * We start by determining the path, bit count, and method of the
	 * addition.  We do this with a lookup on the address, for the full
	 * address width--that is, not limited to the given prefix length.  As
	 * a result, at some point we will find either a NULL pointer, or a
	 * data node with a width that is at least as large as the given prefix
	 * length.  The NULL case is easy: we EXTEND the tree with our new
	 * entry wherever we ran into the NULL pointer.
	 *
	 * If instead we find a sufficiently wide data node, then we see if it
	 * is a match for the new address.  If so, our new data node should
	 * either be INSERTed between two nodes along the path taken so far, or
	 * REPLACE a link node along that path with the new data node.  If it
	 * it is not a match, then the action to take depends on whether the
	 * first differing bit falls within the given prefix length: if so, we
	 * have to BRANCH along the path, using a link node allocated for that
	 * differing bit; if not, we should use INSERT or REPLACE after all.
	 *
	 * As the only exceptional case, we might in fact find an entry for the
	 * exact same address and prefix length as what is being added.  In the
	 * current design of the routing tree, this is always a failure case.
	 */
	parent = NULL;
	side = 0;
	other_entry = NULL;

	for (node = tree->rtt_root; node != NULL;
	    node = node->rtn_child[side]) {
		if (node->rtn_type == RTNT_DATA) {
			other_entry = (struct rttree_entry *)node;

			bit = rttree_diff(tree, other_entry->rte_addr, addr);

			match = (bit >= node->rtn_bits);

			/* Test whether the exact entry already exists. */
			if (match && node->rtn_bits == prefix)
				return EEXIST;

			/*
			 * Test the INSERT/REPLACE and BRANCH cases.  Note that
			 * this condition is in a terse, optimized form that
			 * does not map directly to the two different cases.
			 */
			if (!match || node->rtn_bits > prefix) {
				if (bit > prefix)
					bit = prefix;
				break;
			}
		}

		parent = node;
		side = rttree_side(tree, node, addr);
	}

	/*
	 * At this point, addition is going to succeed no matter what.  Start
	 * by initializing part of 'entry'.  In particular, add the given
	 * entry's link node to the list of free link nodes, because the common
	 * case is that we end up not using it.  If we do, we will just take it
	 * off again right away.  The entry's data node will be initialized as
	 * part of the addition process below.
	 */
	entry->rte_addr = addr;
	entry->rte_mask = mask;

	rttree_add_free(tree, &entry->rte_link);

	/*
	 * First deal with the EXTEND case.  In that case we already know the
	 * intended parent and the side (left/right) for the addition.
	 */
	if (node == NULL) {
		assert(parent == NULL || parent->rtn_bits < prefix);
		assert(parent == NULL || parent->rtn_child[side] == NULL);

		rttree_set(tree, &entry->rte_data, RTNT_DATA, prefix, parent,
		    side, NULL /*left*/, NULL /*right*/);

		return OK;
	}

	/*
	 * For the other three cases, we now have to walk back along the path
	 * we have taken so far in order to find the correct insertion point.
	 */
	while (parent != NULL && parent->rtn_bits >= bit) {
		node = parent;

		parent = node->rtn_parent;
	}

	if (bit == prefix && node->rtn_bits == bit) {
		/*
		 * The REPLACE case.  Replace the link node 'node' with our new
		 * entry.  Afterwards, mark the link node as free.
		 */
		assert(node->rtn_type != RTNT_DATA);

		rttree_replace(tree, node, &entry->rte_data, RTNT_DATA);

		rttree_add_free(tree, node);
	} else if (bit == prefix) {
		/*
		 * The INSERT case.  Insert the data node between 'parent' and
		 * 'node'.  Note that 'parent' may be NULL.  We need to use the
		 * address we found earlier, as 'other_entry', to determine
		 * whether we should add 'node' to the left or right of the
		 * inserted data node.
		 */
		assert(node->rtn_bits > bit);
		assert(parent == NULL || parent->rtn_bits < bit);
		assert(other_entry != NULL);

		side = (parent != NULL && parent->rtn_child[1] == node);

		side2 = rttree_test(tree, other_entry->rte_addr, bit);

		rttree_set(tree, &entry->rte_data, RTNT_DATA, prefix, parent,
		    side, (!side2) ? node : NULL, (side2) ? node : NULL);
	} else {
		/*
		 * The BRANCH case.  In this case, it is impossible that we
		 * find a link node with a bit count equal to the first
		 * differing bit between the address we found and the address
		 * we want to insert: if such a node existed, we would have
		 * descended down its other child during the initial lookup.
		 *
		 * Interpose a link node between 'parent' and 'current' for bit
		 * 'bit', with its other child set to point to 'entry'.  Again,
		 * we need to perform an additional bit test here, because even
		 * though we know that the address we found during the lookup
		 * differs from the given address at bit 'bit', we do not know
		 * the value of either bit yet.
		 */
		assert(bit < prefix);
		assert(node->rtn_bits > bit);
		assert(parent == NULL || parent->rtn_bits < bit);

		link = rttree_get_free(tree);

		side = (parent != NULL && parent->rtn_child[1] == node);

		side2 = rttree_test(tree, addr, bit);

		/* Use NULL for the data node we are about to add. */
		rttree_set(tree, link, RTNT_LINK, bit, parent, side,
		    (side2) ? node : NULL, (!side2) ? node : NULL);

		/* This addition will replace the NULL pointer again. */
		rttree_set(tree, &entry->rte_data, RTNT_DATA, prefix, link,
		    side2, NULL /*left*/, NULL /*right*/);
	}

	return OK;
}

/*
 * Remove a particular node 'node' from the routing tree 'tree'.  The given
 * node must have zero or one children.  As integrity check only, if 'nonempty'
 * is set, the node must have one child.  If the node has one child, that child
 * will be linked to the node's parent (or the tree root), thus cutting the
 * node itself out of the tree.  If the node has zero children, the
 * corresponding slot in its parent (or the tree root) will be cleared.  The
 * function will return a pointer to the parent node if it too qualifies for
 * removal afterwards, or NULL if no further removal action needs to be taken.
 */
static struct rttree_node *
rttree_remove(struct rttree * tree, struct rttree_node * node,
	int nonempty __unused)
{
	struct rttree_node *parent, *child;
	unsigned int side;

	if ((child = node->rtn_child[0]) == NULL)
		child = node->rtn_child[1];

	assert(child != NULL || !nonempty);

	if ((parent = node->rtn_parent) != NULL) {
		side = (parent->rtn_child[1] == node);

		parent->rtn_child[side] = child;

		if (child != NULL)
			child->rtn_parent = parent;
		else if (parent->rtn_type == RTNT_LINK)
			return parent;
	} else {
		tree->rtt_root = child;

		if (child != NULL)
			child->rtn_parent = NULL;
	}

	return NULL;
}

/*
 * Delete the routing entry 'entry' from the routing tree 'tree'.  The entry
 * must have been added before.  This function always succeeds.
 */
void
rttree_delete(struct rttree * tree, struct rttree_entry * entry)
{
	struct rttree_node *node, *link;

	/*
	 * Remove the data node from the tree.  If the data node also has two
	 * children, we have to replace it with a link node.  Otherwise, we
	 * have to remove it and, if it has no children at all, possibly remove
	 * its parent as well.
	 */
	node = &entry->rte_data;

	assert(node->rtn_type == RTNT_DATA);

	if (node->rtn_child[0] != NULL && node->rtn_child[1] != NULL) {
		/*
		 * The link node we allocate here may actually be the entry's
		 * own link node.  We do not make an exception for that case
		 * here, as we have to deal with the entry's link node being in
		 * use a bit further down anyway.
		 */
		link = rttree_get_free(tree);

		rttree_replace(tree, node, link, RTNT_LINK);
	} else {
		/*
		 * Remove the data node from the tree.  If the node has no
		 * children, its removal may leave a link node with one child.
		 * That would be its original parent.  That node must then also
		 * be removed from the tree, and freed up.
		 */
		link = rttree_remove(tree, node, FALSE /*nonempty*/);

		if (link != NULL) {
			(void)rttree_remove(tree, link, TRUE /*nonempty*/);

			rttree_add_free(tree, link);
		}
	}

	/*
	 * Remove the entry's link node from either the tree or the free list,
	 * depending on the type currently assigned to it.  If it has to be
	 * removed from the tree, it must be replaced with another link node.
	 * There will always be enough link nodes available for this to work.
	 */
	node = &entry->rte_link;

	if (node->rtn_type == RTNT_LINK) {
		link = rttree_get_free(tree);

		rttree_replace(tree, node, link, RTNT_LINK);
	} else {
		assert(node->rtn_type == RTNT_FREE);

		rttree_del_free(tree, node);
	}
}
