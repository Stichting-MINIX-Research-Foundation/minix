/* VTreeFS - inode.c - inode management */

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
		assert((unsigned int)(node - &inode[0]) == node->i_num);\
		assert(node == &inode[0] || node->i_parent != NULL ||	\
		    (node->i_flags & I_DELETED));			\
	} while (0);

/*
 * Initialize the inode-related state.
 */
int
init_inodes(unsigned int inodes, struct inode_stat * istat,
	index_t nr_indexed_entries)
{
	struct inode *node;
	unsigned int i;

	assert(inodes > 0);
	assert(nr_indexed_entries >= 0);

	nr_inodes = inodes;

	/* Allocate the inode and hash tables. */
	if ((inode = malloc(nr_inodes * sizeof(inode[0]))) == NULL)
		return ENOMEM;

	parent_name_head = malloc(nr_inodes * sizeof(parent_name_head[0]));
	if (parent_name_head == NULL) {
		free(inode);
		return ENOMEM;
	}

	parent_index_head = malloc(nr_inodes * sizeof(parent_index_head[0]));
	if (parent_index_head == NULL) {
		free(parent_name_head);
		free(inode);
		return ENOMEM;
	}

#if DEBUG
	printf("VTREEFS: allocated %zu+%zu+%zu bytes\n",
	    nr_inodes * sizeof(inode[0]),
	    nr_inodes * sizeof(parent_name_head[0]),
	    nr_inodes * sizeof(parent_index_head[0]));
#endif

	/* Initialize the free/unused list. */
	TAILQ_INIT(&unused_inodes);

	/* Add free inodes to the unused/free list.  Skip the root inode. */
	for (i = 1; i < nr_inodes; i++) {
		node = &inode[i];
		node->i_num = i;
		node->i_name = NULL;
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
	node->i_num = 0;
	node->i_parent = NULL;
	node->i_count = 0;
	TAILQ_INIT(&node->i_children);
	node->i_flags = 0;
	node->i_index = NO_INDEX;
	set_inode_stat(node, istat);
	node->i_indexed = nr_indexed_entries;
	node->i_cbdata = NULL;

	return OK;
}

/*
 * Clean up the inode-related state.
 */
void
cleanup_inodes(void)
{

	/* Free the inode and hash tables. */
	free(parent_index_head);
	free(parent_name_head);
	free(inode);
}

/*
 * Return the hash value of <parent,name> tuple.
 */
static int
parent_name_hash(const struct inode * parent, const char *name)
{
	unsigned int name_hash;

	/* Use the sdbm algorithm to hash the name. */
	name_hash = sdbm_hash(name, strlen(name));

	/* The parent hash is a simple array entry. */
	return (parent->i_num ^ name_hash) % nr_inodes;
}

/*
 * Return the hash value of a <parent,index> tuple.
 */
static int
parent_index_hash(const struct inode * parent, index_t idx)
{

	return (parent->i_num ^ idx) % nr_inodes;
}

/*
 * Delete a deletable inode to make room for a new inode.
 */
static void
purge_inode(struct inode * parent)
{
	/*
	 * An inode is deletable if:
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
	unsigned int count;

	assert(TAILQ_EMPTY(&unused_inodes));

	/*
	 * This should not happen often enough to warrant an extra linked list,
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

/*
 * Add an inode.
 */
struct inode *
add_inode(struct inode * parent, const char * name, index_t idx,
	const struct inode_stat * istat, index_t nr_indexed_entries,
	cbdata_t cbdata)
{
	struct inode *newnode;
	char *newname;
	int slot;

	CHECK_INODE(parent);
	assert(S_ISDIR(parent->i_stat.mode));
	assert(!(parent->i_flags & I_DELETED));
	assert(strlen(name) <= NAME_MAX);
	assert(idx >= 0 || idx == NO_INDEX);
	assert(istat != NULL);
	assert(nr_indexed_entries >= 0);
	assert(get_inode_by_name(parent, name) == NULL);

	/* Get a free inode.  Free one up if necessary. */
	if (TAILQ_EMPTY(&unused_inodes))
		purge_inode(parent);

	assert(!TAILQ_EMPTY(&unused_inodes));

	newnode = TAILQ_FIRST(&unused_inodes);

	/* Use the static name buffer if the name is short enough. Otherwise,
	 * allocate heap memory for the name.
	 */
	newname = newnode->i_namebuf;
	if (strlen(name) > PNAME_MAX &&
	    (newname = malloc(strlen(name) + 1)) == NULL)
		return NULL;

	TAILQ_REMOVE(&unused_inodes, newnode, i_unused);

	assert(newnode->i_count == 0);

	/* Copy the relevant data to the inode. */
	newnode->i_parent = parent;
	newnode->i_name = newname;
	newnode->i_flags = 0;
	newnode->i_index = idx;
	newnode->i_stat = *istat;
	newnode->i_indexed = nr_indexed_entries;
	newnode->i_cbdata = cbdata;
	strcpy(newnode->i_name, name);

	/* Clear the extra data for this inode, if present. */
	clear_inode_extra(newnode);

	/* Add the inode to the list of children inodes of the parent. */
	TAILQ_INSERT_HEAD(&parent->i_children, newnode, i_siblings);

	/* Add the inode to the <parent,name> hash table. */
	slot = parent_name_hash(parent, name);
	LIST_INSERT_HEAD(&parent_name_head[slot], newnode, i_hname);

	/* Add the inode to the <parent,index> hash table. */
	if (idx != NO_INDEX) {
		slot = parent_index_hash(parent, idx);
		LIST_INSERT_HEAD(&parent_index_head[slot], newnode, i_hindex);
	}

	return newnode;
}

/*
 * Return the file system's root inode.
 */
struct inode *
get_root_inode(void)
{

	/* The root node is always the first node in the inode table. */
	return &inode[0];
}

/*
 * Return the name that an inode has in its parent directory.
 */
const char *
get_inode_name(const struct inode * node)
{

	CHECK_INODE(node);
	assert(!(node->i_flags & I_DELETED));
	assert(node->i_name != NULL);

	return node->i_name;
}

/*
 * Return the index that an inode has in its parent directory.
 */
index_t
get_inode_index(const struct inode * node)
{

	CHECK_INODE(node);

	return node->i_index;
}

/*
 * Return the number of indexed slots for the given (directory) inode.
 */
index_t
get_inode_slots(const struct inode * node)
{

	CHECK_INODE(node);

	return node->i_indexed;
}

/*
 * Return the callback data associated with the given inode.
 */
cbdata_t
get_inode_cbdata(const struct inode * node)
{

	CHECK_INODE(node);

	return node->i_cbdata;
}

/*
 * Return an inode's parent inode.
 */
struct inode *
get_parent_inode(const struct inode * node)
{

	CHECK_INODE(node);

	/* The root inode does not have parent. */
	if (node == &inode[0])
		return NULL;

	return node->i_parent;
}

/*
 * Return a directory's first (non-deleted) child inode.
 */
struct inode *
get_first_inode(const struct inode * parent)
{
	struct inode *node;

	CHECK_INODE(parent);
	assert(S_ISDIR(parent->i_stat.mode));

	node = TAILQ_FIRST(&parent->i_children);

	while (node != NULL && (node->i_flags & I_DELETED))
		node = TAILQ_NEXT(node, i_siblings);

	return node;
}

/*
 * Return a directory's next (non-deleted) child inode.
 */
struct inode *
get_next_inode(const struct inode * previous)
{
	struct inode *node;

	CHECK_INODE(previous);

	node = TAILQ_NEXT(previous, i_siblings);

	while (node != NULL && (node->i_flags & I_DELETED))
		node = TAILQ_NEXT(node, i_siblings);

	return node;
}

/*
 * Return the inode number of the given inode.
 */
int
get_inode_number(const struct inode * node)
{

	CHECK_INODE(node);

	return node->i_num + 1;
}

/*
 * Retrieve an inode's status.
 */
void
get_inode_stat(const struct inode * node, struct inode_stat * istat)
{

	CHECK_INODE(node);

	*istat = node->i_stat;
}

/*
 * Set an inode's status.
 */
void
set_inode_stat(struct inode * node, struct inode_stat * istat)
{

	CHECK_INODE(node);

	node->i_stat = *istat;
}

/*
 * Look up an inode using a <parent,name> tuple.
 */
struct inode *
get_inode_by_name(const struct inode * parent, const char * name)
{
	struct inode *node;
	int slot;

	CHECK_INODE(parent);
	assert(strlen(name) <= NAME_MAX);
	assert(S_ISDIR(parent->i_stat.mode));

	/* Get the hash value, and search for the inode. */
	slot = parent_name_hash(parent, name);
	LIST_FOREACH(node, &parent_name_head[slot], i_hname) {
		if (parent == node->i_parent && !strcmp(name, node->i_name))
			return node;	/* found */
	}

	return NULL;
}

/*
 * Look up an inode using a <parent,index> tuple.
 */
struct inode *
get_inode_by_index(const struct inode * parent, index_t idx)
{
	struct inode *node;
	int slot;

	CHECK_INODE(parent);
	assert(S_ISDIR(parent->i_stat.mode));
	assert(idx >= 0);

	if (idx >= parent->i_indexed)
		return NULL;

	/* Get the hash value, and search for the inode. */
	slot = parent_index_hash(parent, idx);
	LIST_FOREACH(node, &parent_index_head[slot], i_hindex) {
		if (parent == node->i_parent && idx == node->i_index)
			return node;	/* found */
	}

	return NULL;
}

/*
 * Retrieve an inode by inode number.
 */
struct inode *
find_inode(ino_t num)
{
	struct inode *node;

	node = &inode[num - 1];

	CHECK_INODE(node);

	return node;
}

/*
 * Retrieve an inode by inode number, and increase its reference count.
 */
struct inode *
get_inode(ino_t num)
{
	struct inode *node;

	if ((node = find_inode(num)) == NULL)
		return NULL;

	node->i_count++;
	return node;
}

/*
 * Decrease an inode's reference count.
 */
void
put_inode(struct inode * node)
{

	CHECK_INODE(node);
	assert(node->i_count > 0);

	node->i_count--;

	/*
	 * If the inode is scheduled for deletion, and has no more references,
	 * actually delete it now.
	 */
	if ((node->i_flags & I_DELETED) && node->i_count == 0)
		delete_inode(node);
}

/*
 * Increase an inode's reference count.
 */
void
ref_inode(struct inode * node)
{

	CHECK_INODE(node);

	node->i_count++;
}

/*
 * Unlink the given node from its parent, if it is still linked in.
 */
static void
unlink_inode(struct inode * node)
{
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

/*
 * Delete the given inode.  If its reference count is nonzero, or it still has
 * children that cannot be deleted for the same reason, keep the inode around
 * for the time being.  If the node is a directory, keep around its parent so
 * that we can still do a "cd .." out of it.  For these reasons, this function
 * may be called on an inode more than once before it is actually deleted.
 */
void
delete_inode(struct inode * node)
{
	struct inode *cnode, *ctmp;

	CHECK_INODE(node);
	assert(node != &inode[0]);

	/*
	 * If the inode was not already scheduled for deletion, partially
	 * remove the node.
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

		/* Free the name if allocated dynamically. */
		assert(node->i_name != NULL);
		if (node->i_name != node->i_namebuf)
			free(node->i_name);
		node->i_name = NULL;

		node->i_flags |= I_DELETED;

		/*
		 * If this inode is not a directory, we don't care about being
		 * able to find its parent.  Unlink it from the parent now.
		 */
		if (!S_ISDIR(node->i_stat.mode))
			unlink_inode(node);
	}

	if (node->i_count == 0 && TAILQ_EMPTY(&node->i_children)) {
		/*
		 * If this inode still has a parent at this point, unlink it
		 * now; noone can possibly refer to it anymore.
		 */
		if (node->i_parent != NULL)
			unlink_inode(node);

		/* Delete the actual node. */
		TAILQ_INSERT_HEAD(&unused_inodes, node, i_unused);
	}
}

/*
 * Return whether the given inode has been deleted.
 */
int
is_inode_deleted(const struct inode * node)
{

	return (node->i_flags & I_DELETED);
}

/*
 * Find the inode specified by the request message, and decrease its reference
 * count.
 */
int
fs_putnode(ino_t ino_nr, unsigned int count)
{
	struct inode *node;

	/* Get the inode specified by its number. */
	if ((node = find_inode(ino_nr)) == NULL)
		return EINVAL;

	/* Decrease the reference count. */
	assert(node->i_count >= count);

	node->i_count -= count - 1;
	put_inode(node);

	return OK;
}
