/* PTYFS slave node management */
/*
 * While the interface of this module should be flexible enough to implement
 * various memory management approaches, the current code simply relies on
 * NR_PTYS being small enough to preallocate all data structures.  In the
 * future, NR_PTYS will no longer be a system-global definition, and future
 * implementations of this module should not rely on NR_PTYS at all.
 */

#include <minix/drivers.h>

#include "node.h"

static bitchunk_t node_map[BITMAP_CHUNKS(NR_PTYS)];
static struct node_data node_data[NR_PTYS];

/*
 * Initialize the node module.
 */
void
init_nodes(void)
{

	memset(&node_map, 0, sizeof(node_map));
}

/*
 * Allocate a node with a given node index number, and save node data for it.
 * It is possible that the node is in use already; in that case, only update
 * its associated data.  Return OK on success, or an error code on failure.
 */
int
set_node(node_t index, struct node_data * data)
{

	if (index >= NR_PTYS)
		return ENOMEM;

	SET_BIT(node_map, index);

	node_data[index] = *data;

	return OK;
}

/*
 * Deallocate a node using its node index number.  This function always
 * succeeds, intentionally ignoring the case that the node was not allocated.
 */
void
clear_node(node_t index)
{

	UNSET_BIT(node_map, index);
}

/*
 * Return a pointer to the node data associated with the given node index
 * number.  If the node is not allocated, return NULL.
 */
struct node_data *
get_node(node_t index)
{

	if (index >= NR_PTYS || !GET_BIT(node_map, index))
		return NULL;

	return &node_data[index];
}

/*
 * Return the highest allocated node index number, plus one.  This value is
 * used to check given node indices and limit linear iterations.
 */
node_t
get_max_node(void)
{

	/*
	 * NR_PTYS is low enough that we can always return it instead of
	 * tracking the actual value.
	 */
	return NR_PTYS;
}
