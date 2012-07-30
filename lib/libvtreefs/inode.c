/* VTreeFS - inode.c - by Alen Stojanov and David van Moolenbroek */

#include "inc.h"

/* The number of inodes and hash table slots. */
static unsigned int nr_inodes;

/* The table of all the inodes. */
static struct inode *inode;

/* The list of unused inodes. */
static TAILQ_HEAD(unused_head, inode) unused_inodes;

/* The hash tables for lookup of <parent,name> and <parent,index> to inode. */
static LIST_HEAD(name_head, inode) *parent_name_head;
static LIST_HEAD(index_head, inode) *parent_index_head;

/* Internal integrity check. */
#define CHECK_INODE(node)						\
	do {								\
		assert(node >= &inode[0] && node < &inode[nr_inodes]);	\
		assert(node == &inode[0] ||				\
			node->i_parent != NULL ||			\
			(node->i_flags & I_DELETED));			\
	} while (0);

/*===========================================================================*
 *				init_inodes				     *
 *===========================================================================*/
void init_inodes(unsigned int inodes, struct inode_stat *stat,
	index_t nr_indexed_entries)
{
	/* Initialize the inode-related state.
	 */
	struct inode *node;
	int i;

	assert(inodes > 0);
	assert(nr_indexed_entries >= 0);

	nr_inodes = inodes;

	/* Allocate the inode and hash tables. */
	inode = malloc(nr_inodes * sizeof(inode[0]));
	parent_name_head = malloc(nr_inodes * sizeof(parent_name_head[0]));
	parent_index_head = malloc(nr_inodes * sizeof(parent_index_head[0]));

	assert(inode != NULL);
	assert(parent_name_head != NULL);
	assert(parent_index_head != NULL);

#if DEBUG
	printf("VTREEFS: allocated %d+%d+%d bytes\n",
		nr_inodes * sizeof(inode[0]),
		nr_inodes * sizeof(parent_name_head[0]),
		nr_inodes * sizeof(parent_index_head[0]));
#endif

	/* Initialize the free/unused list. */
	TAILQ_INIT(&unused_inodes);

	/* Add free inodes to the unused/free list. Skip the root inode. */
	for (i = 1; i < nr_inodes; i++) {
		node = &inode[i];

		node->i_parent = NULL;
		node->i_count = 0;
		TAILQ_INIT(&node->i_children);
		TAILQ_INSERT_HEAD(&unused_inodes, node, i_unused);
	}

	/* Initialize the hash lists. */
	for (i = 0; i < nr_inodes; i++) {
		LIST_INIT(&parent_name_head[i]);
		LIST_INIT(&parent_index_head[i]);
	}

	/* Initialize the root inode. */
	node = &inode[0];
	node->i_parent = NULL;
	node->i_count = 0;
	TAILQ_INIT(&node->i_children);
	node->i_flags = 0;
	node->i_index = NO_INDEX;
	set_inode_stat(node, stat);
	node->i_indexed = nr_indexed_entries;
	node->i_cbdata = NULL;
}

/*===========================================================================*
 *				cleanup_inodes				     *
 *===========================================================================*/
void cleanup_inodes(void)
{
	/* Clean up the inode-related state.
	 */

	/* Free the inode and hash tables. */
	free(parent_index_head);
	free(parent_name_head);
	free(inode);
}

/*===========================================================================*
 *				parent_name_hash			     *
 *===========================================================================*/
static int parent_name_hash(struct inode *parent, char *name)
{
	/* Return the hash value of <parent,name> tuple.
	 */
	unsigned int name_hash;
	unsigned long parent_hash;

	/* The parent hash is a simple array entry; find its index. */
	parent_hash = parent - &inode[0];

	/* Use the sdbm algorithm to hash the name. */
	name_hash = sdbm_hash(name, strlen(name));

	return (parent_hash ^ name_hash) % nr_inodes;
}

/*===========================================================================*
 *				parent_index_hash			     *
 *===========================================================================*/
static int parent_index_hash(struct inode *parent, index_t index)
{
	/* Return the hash value of a <parent,index> tuple.
	 */

	return ((parent - &inode[0]) ^ index) % nr_inodes;
}

/*===========================================================================*
 *				purge_inode				     *
 *===========================================================================*/
void purge_inode(struct inode *parent)
{
	/* Delete a deletable inode to make room for a new inode.
	 */
	/* An inode is deletable if:
	 * - it is in use;
	 * - it is indexed;
	 * - it is not the given parent inode;
	 * - it has a zero reference count;
	 * - it does not have any children.
	 * The first point is true for all inodes, or we would not be here.
	 * The latter two points also imply that I_DELETED is not set.
	 */
	static int last_checked = 0;
	struct inode *node;
	int count;

	assert(TAILQ_EMPTY(&unused_inodes));

	/* This should not happen often enough to warrant an extra linked list,
	 * especially as maintenance of that list would be rather error-prone..
	 */
	for (count = 0; count < nr_inodes; count++) {
		node = &inode[last_checked];
		last_checked = (last_checked + 1) % nr_inodes;

		if (node != parent && node->i_index != NO_INDEX &&
			node->i_count == 0 && TAILQ_EMPTY(&node->i_children)) {

			assert(!(node->i_flags & I_DELETED));

			delete_inode(node);

			break;
		}
	}
}

/*===========================================================================*
 *				add_inode				     *
 *===========================================================================*/
struct inode *add_inode(struct inode *parent, char *name,
	index_t index, struct inode_stat *stat, index_t nr_indexed_entries,
	cbdata_t cbdata)
{
	/* Add an inode.
	 */
	struct inode *newnode;
	int slot;

	CHECK_INODE(parent);
	assert(S_ISDIR(parent->i_stat.mode));
	assert(!(parent->i_flags & I_DELETED));
	assert(strlen(name) <= PNAME_MAX);
	assert(index >= 0 || index == NO_INDEX);
	assert(stat != NULL);
	assert(nr_indexed_entries >= 0);
	assert(get_inode_by_name(parent, name) == NULL);

	/* Get a free inode. Free one up if necessary. */
	if (TAILQ_EMPTY(&unused_inodes))
		purge_inode(parent);

	assert(!TAILQ_EMPTY(&unused_inodes));

	newnode = TAILQ_FIRST(&unused_inodes);
	TAILQ_REMOVE(&unused_inodes, newnode, i_unused);

	assert(newnode->i_count == 0);

	/* Copy the relevant data to the inode. */
	newnode->i_parent = parent;
	newnode->i_flags = 0;
	newnode->i_index = index;
	newnode->i_stat = *stat;
	newnode->i_indexed = nr_indexed_entries;
	newnode->i_cbdata = cbdata;
	strlcpy(newnode->i_name, name, sizeof(newnode->i_name));

	/* Add the inode to the list of children inodes of the parent. */
	TAILQ_INSERT_HEAD(&parent->i_children, newnode, i_siblings);

	/* Add the inode to the <parent,name> hash table. */
	slot = parent_name_hash(parent, name);
	LIST_INSERT_HEAD(&parent_name_head[slot], newnode, i_hname);

	/* Add the inode to the <parent,index> hash table. */
	if (index != NO_INDEX) {
		slot = parent_index_hash(parent, index);
		LIST_INSERT_HEAD(&parent_index_head[slot], newnode, i_hindex);
	}

	return newnode;
}

/*===========================================================================*
 *				get_root_inode				     *
 *===========================================================================*/
struct inode *get_root_inode(void)
{
	/* Return the file system's root inode.
	 */

	/* The root node is always the first node in the inode table */
	return &inode[0];
}

/*===========================================================================*
 *				get_inode_name				     *
 *===========================================================================*/
char const *get_inode_name(struct inode *node)
{
	/* Return the name that an inode has in its parent directory.
	 */

	CHECK_INODE(node);

	return node->i_name;
}

/*===========================================================================*
 *				get_inode_index				     *
 *===========================================================================*/
index_t get_inode_index(struct inode *node)
{
	/* Return the index that an inode has in its parent directory.
	 */

	CHECK_INODE(node);

	return node->i_index;
}

/*===========================================================================*
 *				get_inode_cbdata			     *
 *===========================================================================*/
cbdata_t get_inode_cbdata(struct inode *node)
{
	/* Return the callback data associated with the given inode.
	 */

	CHECK_INODE(node);

	return node->i_cbdata;
}

/*===========================================================================*
 *				get_parent_inode			     *
 *===========================================================================*/
struct inode *get_parent_inode(struct inode *node)
{
	/* Return an inode's parent inode.
	 */

	CHECK_INODE(node);

	/* The root inode does not have parent. */
	if (node == &inode[0])
		return NULL;

	return node->i_parent;
}

/*===========================================================================*
 *				get_first_inode				     *
 *===========================================================================*/
struct inode *get_first_inode(struct inode *parent)
{
	/* Return a directory's first (non-deleted) child inode.
	 */
	struct inode *node;

	CHECK_INODE(parent);
	assert(S_ISDIR(parent->i_stat.mode));

	node = TAILQ_FIRST(&parent->i_children);

	while (node != NULL && (node->i_flags & I_DELETED))
		node = TAILQ_NEXT(node, i_siblings);

	return node;
}

/*===========================================================================*
 *				get_next_inode				     *
 *===========================================================================*/
struct inode *get_next_inode(struct inode *previous)
{
	/* Return a directory's next (non-deleted) child inode.
	 */
	struct inode *node;

	CHECK_INODE(previous);

	node = TAILQ_NEXT(previous, i_siblings);

	while (node != NULL && (node->i_flags & I_DELETED))
		node = TAILQ_NEXT(node, i_siblings);

	return node;
}

/*===========================================================================*
 *				get_inode_number			     *
 *===========================================================================*/
int get_inode_number(struct inode *node)
{
	/* Return the inode number of the given inode.
	 */

	CHECK_INODE(node);

	return (int) (node - &inode[0]) + 1;
}

/*===========================================================================*
 *				get_inode_stat				     *
 *===========================================================================*/
void get_inode_stat(struct inode *node, struct inode_stat *stat)
{
	/* Retrieve an inode's status.
	 */

	CHECK_INODE(node);

	*stat = node->i_stat;
}

/*===========================================================================*
 *				set_inode_stat				     *
 *===========================================================================*/
void set_inode_stat(struct inode *node, struct inode_stat *stat)
{
	/* Set an inode's status.
	 */

	CHECK_INODE(node);

	node->i_stat = *stat;
}

/*===========================================================================*
 *				get_inode_by_name			     *
 *===========================================================================*/
struct inode *get_inode_by_name(struct inode *parent, char *name)
{
	/* Look up an inode using a <parent,name> tuple.
	 */
	struct inode *node;
	int slot;

	CHECK_INODE(parent);
	assert(strlen(name) <= PNAME_MAX);
	assert(S_ISDIR(parent->i_stat.mode));

	/* Get the hash value, and search for the inode. */
	slot = parent_name_hash(parent, name);
	LIST_FOREACH(node, &parent_name_head[slot], i_hname) {
		if (parent == node->i_parent && !strcmp(name, node->i_name))
			return node;	/* found */
	}

	return NULL;
}

/*===========================================================================*
 *				get_inode_by_index			     *
 *===========================================================================*/
struct inode *get_inode_by_index(struct inode *parent, index_t index)
{
	/* Look up an inode using a <parent,index> tuple.
	 */
	struct inode *node;
	int slot;

	CHECK_INODE(parent);
	assert(S_ISDIR(parent->i_stat.mode));
	assert(index >= 0 && index < parent->i_indexed);

	/* Get the hash value, and search for the inode. */
	slot = parent_index_hash(parent, index);
	LIST_FOREACH(node, &parent_index_head[slot], i_hindex) {
		if (parent == node->i_parent && index == node->i_index)
			return node;	/* found */
	}

	return NULL;
}

/*===========================================================================*
 *				find_inode				     *
 *===========================================================================*/
struct inode *find_inode(ino_t num)
{
	/* Retrieve an inode by inode number.
	 */
	struct inode *node;

	node = &inode[num - 1];

	CHECK_INODE(node);

	return node;
}

/*===========================================================================*
 *				get_inode				     *
 *===========================================================================*/
struct inode *get_inode(ino_t num)
{
	/* Retrieve an inode by inode number, and increase its reference count.
	 */
	struct inode *node;

	if ((node = find_inode(num)) == NULL)
		return NULL;

	node->i_count++;
	return node;
}

/*===========================================================================*
 *				put_inode				     *
 *===========================================================================*/
void put_inode(struct inode *node)
{
	/* Decrease an inode's reference count.
	 */

	CHECK_INODE(node);
	assert(node->i_count > 0);

	node->i_count--;

	/* If the inode is scheduled for deletion, and has no more references,
	 * actually delete it now.
	 */
	if ((node->i_flags & I_DELETED) && node->i_count == 0)
		delete_inode(node);
}

/*===========================================================================*
 *				ref_inode				     *
 *===========================================================================*/
void ref_inode(struct inode *node)
{
	/* Increase an inode's reference count.
	 */

	CHECK_INODE(node);
	assert(node->i_count >= 0);

	node->i_count++;
}

/*===========================================================================*
 *				unlink_inode				     *
 *===========================================================================*/
static void unlink_inode(struct inode *node)
{
	/* Unlink the given node from its parent, if it is still linked in.
	 */
	struct inode *parent;

	assert(node->i_flags & I_DELETED);

	parent = node->i_parent;
	if (parent == NULL)
		return;

	/* Delete the node from the parent list. */
	node->i_parent = NULL;

	TAILQ_REMOVE(&parent->i_children, node, i_siblings);

	/* Optionally recheck if the parent can now be deleted. */
	if (parent->i_flags & I_DELETED)
		delete_inode(parent);
}

/*===========================================================================*
 *				delete_inode				     *
 *===========================================================================*/
void delete_inode(struct inode *node)
{
	/* Delete the given inode. If its reference count is nonzero, or it
	 * still has children that cannot be deleted for the same reason, keep
	 * the inode around for the time being. If the node is a directory,
	 * keep around its parent so that we can still do a "cd .." out of it.
	 * For these reasons, this function may be called on an inode more than
	 * once before it is actually deleted.
	 */
	struct inode *cnode, *ctmp;

	CHECK_INODE(node);
	assert(node != &inode[0]);

	/* If the inode was not already scheduled for deletion,
	 * partially remove the node.
	 */
	if (!(node->i_flags & I_DELETED)) {
		/* Remove any children first (before I_DELETED is set!). */
		TAILQ_FOREACH_SAFE(cnode, &node->i_children, i_siblings, ctmp)
			delete_inode(cnode);

		/* Unhash the inode from the <parent,name> table. */
		LIST_REMOVE(node, i_hname);

		/* Unhash the inode from the <parent,index> table if needed. */
		if (node->i_index != NO_INDEX)
			LIST_REMOVE(node, i_hindex);

		node->i_flags |= I_DELETED;

		/* If this inode is not a directory, we don't care about being
		 * able to find its parent. Unlink it from the parent now.
		 */
		if (!S_ISDIR(node->i_stat.mode))
			unlink_inode(node);
	}

	if (node->i_count == 0 && TAILQ_EMPTY(&node->i_children)) {
		/* If this inode still has a parent at this point, unlink it
		 * now; noone can possibly refer to it anymore.
		 */
		if (node->i_parent != NULL)
			unlink_inode(node);

		/* Delete the actual node. */
		TAILQ_INSERT_HEAD(&unused_inodes, node, i_unused);
	}
}

/*===========================================================================*
 *				is_inode_deleted			     *
 *===========================================================================*/
int is_inode_deleted(struct inode *node)
{
	/* Return whether the given inode has been deleted.
	 */

	return (node->i_flags & I_DELETED);
}

/*===========================================================================*
 *				fs_putnode				     *
 *===========================================================================*/
int fs_putnode(void)
{
	/* Find the inode specified by the request message, and decrease its
	 * reference count.
	 */
	struct inode *node;

	/* Get the inode specified by its number. */
	if ((node = find_inode(fs_m_in.REQ_INODE_NR)) == NULL)
		return EINVAL;

	/* Decrease the reference count. */
	node->i_count -= fs_m_in.REQ_COUNT - 1;

	assert(node->i_count > 0);

	put_inode(node);

	return OK;
}
