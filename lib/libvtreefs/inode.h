#ifndef _VTREEFS_INODE_H
#define _VTREEFS_INODE_H

/* The inodes that are active, form a fully connected tree. Each node except
 * the root node has a parent and a tail queue of children, where each child
 * inode points to the "next" and "previous" inode with a common parent.
 *
 * Each inode that has a parent (i.e. active and not the root), is part of a
 * <parent,name> -> inode hashtable, and if it has an index into the parent,
 * is part of a <parent,index> -> inode hashtable.
 *
 * Inodes that are not active, are either deleted or free. A deleted inode is
 * in use as long as it still has a nonzero reference count, even though it is
 * no longer part of the tree. Inodes that are free, are part of the list of
 * unused inodes.
 */
struct inode {
	/* Inode metadata */
	struct inode_stat i_stat;	/* POSIX attributes */
	char i_name[PNAME_MAX + 1];	/* name of the inode in the parent */
	int i_count;			/* reference count */
	index_t i_index;		/* index number in parent / NO_INDEX */
	int i_indexed;			/* number of indexed entries */
	cbdata_t i_cbdata;		/* callback data */
	unsigned short i_flags;		/* I_DELETED or 0 */

	/* Tree structure */
	struct inode *i_parent;		/* parent of the node */
	TAILQ_ENTRY(inode) i_siblings;	/* hash list for parent's children */
	TAILQ_HEAD(i_child, inode) i_children;	/* parent's children */

	/* Hash/free structure */
	LIST_ENTRY(inode) i_hname;	/* hash list for name hash table */
	LIST_ENTRY(inode) i_hindex;	/* hash list for index hash table */
	TAILQ_ENTRY(inode) i_unused;	/* list of unused nodes */
};

#define I_DELETED 	0x1	/* the inode is scheduled for deletion */

#endif /* _VTREEFS_INODE_H */
