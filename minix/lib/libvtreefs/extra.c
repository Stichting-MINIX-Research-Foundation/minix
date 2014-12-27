/* VTreeFS - extra.c - per-inode storage of arbitrary extra data */

#include "inc.h"

/*
 * Right now, we maintain the extra data (if requested) as a separate buffer,
 * so that we don't have to make the inode structure variable in size.  Later,
 * if for example the maximum node name length becomes a runtime setting, we
 * could reconsider this.
 */
static char *extra_ptr = NULL;
static size_t extra_size = 0; /* per inode */

/*
 * Initialize memory to store extra data.
 */
int
init_extra(unsigned int nr_inodes, size_t inode_extra)
{

	if (inode_extra == 0)
		return OK;

	if ((extra_ptr = calloc(nr_inodes, inode_extra)) == NULL)
		return ENOMEM;

	extra_size = inode_extra;

	return OK;
}

/*
 * Initialize the extra data for the given inode to zero.
 */
void
clear_inode_extra(struct inode * node)
{

	if (extra_size == 0)
		return;

	memset(&extra_ptr[node->i_num * extra_size], 0, extra_size);
}

/*
 * Retrieve a pointer to the extra data for the given inode.
 */
void *
get_inode_extra(const struct inode * node)
{

	if (extra_size == 0)
		return NULL;

	return (void *)&extra_ptr[node->i_num * extra_size];
}
