#ifndef _SFFS_INODE_H
#define _SFFS_INODE_H

/* We cannot use inode number 0, so to be able to use bitmasks to combine
 * inode and generation numbers, we have to use one fewer than the maximum of
 * inodes possible by using NUM_INODE_BITS bits.
 */
#define NUM_INODES	((1 << NUM_INODE_BITS) - 1)

/* The main portion of the inode array forms a fully linked tree, providing a
 * cached partial view of what the server believes is on the host system. Each
 * inode contains only a pointer to its parent and its path component name, so
 * a path for an inode is constructed by walking up to the root. Inodes that
 * are in use as directory for a child node must not be recycled; in this case,
 * the i_child list is not empty. Naturally, inodes for which VFS holds a
 * reference must also not be recycled; the i_ref count takes care of that.
 *
 * Multiple hard links to a single file do not exist; that is why an inode is
 * also a directory entry (when in IN USE or CACHED state). Notifications about
 * modifications on the host system are not part of the protocol, so sometimes
 * the server may discover that some files do not exist anymore. In that case,
 * they are marked as DELETED in the inode table. Such files may still be used
 * because of open file handles, but cannot be referenced by path anymore. The
 * underlying protocol may not support truncation of open files anyway. Since
 * we currently cannot guarantee that a file is actually opened before it is
 * deleted (as this would consistute opening every file being looked up), we
 * effectively do not properly support open deleted files at all anyway.
 *
 * An inode is REFERENCED iff it has a reference count > 0 *or* has children.
 * An inode is LINKED IN iff it has a parent.
 *
 * An inode is IN USE iff it is REFERENCED and LINKED IN.
 * An inode is CACHED iff it is NOT REFERENCED and LINKED IN.
 * An inode is DELETED iff it is REFERENCED and NOT LINKED IN.
 * An inode is FREE iff it is NOT REFERENCED and NOT LINKED IN.
 *
 * An inode may have an open file handle if it is IN USE or DELETED.
 * An inode may have children if it is IN USE (and is a directory).
 * An inode is in the names hashtable iff it is IN USE or CACHED.
 * An inode is on the free list iff it is CACHED or FREE.
 *
 * - An IN USE inode becomes DELETED when it is either deleted explicitly, or
 *   when it has been determined to have become unreachable by path name on the 
 *   host system (the verify_* functions take care of this).
 * - An IN USE inode may become CACHED when there are no VFS references to it
 *   anymore (i_ref == 0), and it is not a directory with children.
 * - A DELETED inode cannot have children, but may become FREE when there are
 *   also no VFS references to it anymore.
 * - A CACHED inode may become IN USE when either i_ref or i_link is increased
 *   from zero. Practically, it will always be i_ref that gets increased, since
 *   i_link cannot be increased by VFS without having a reference to the inode.
 * - A CACHED or FREE inode may be reused for other purposes at any time.
 */

struct inode {
  struct inode *i_parent;		/* parent inode pointer */
  LIST_HEAD(child_head, inode) i_child;	/* child inode anchor */
  LIST_ENTRY(inode) i_next;		/* sibling inode chain entry */
  LIST_ENTRY(inode) i_hash;		/* hashtable chain entry */
  unsigned short i_num;			/* inode number for quick reference */
  unsigned short i_gen;			/* inode generation number */
  unsigned short i_ref;			/* VFS reference count */
  unsigned short i_flags;		/* any combination of I_* flags */
  union {
	TAILQ_ENTRY(inode) i_free;	/* free list chain entry */
	sffs_file_t i_file;		/* handle to open file */
	sffs_dir_t i_dir;		/* handle to open directory */
  };
  char i_name[NAME_MAX+1];		/* entry name in parent directory */
};

#define I_DIR		0x01		/* this inode represents a directory */
#define I_HANDLE	0x02		/* this inode has an open handle */

/* warning: the following line is not a proper macro */
#define INODE_NR(i)	(((i)->i_gen << NUM_INODE_BITS) | (i)->i_num)
#define INODE_INDEX(n)	(((n) & ((1 << NUM_INODE_BITS) - 1)) - 1)
#define INODE_GEN(n)	(((n) >> NUM_INODE_BITS) & 0xffff)

#define ROOT_INODE_NR	1

#define IS_DIR(i)	((i)->i_flags & I_DIR)
#define IS_ROOT(i)	((i)->i_num == ROOT_INODE_NR)
#define HAS_CHILDREN(i)	(!LIST_EMPTY(&(i)->i_child))

#define MODE_TO_DIRFLAG(m)	(S_ISDIR(m) ? I_DIR : 0)

#endif /* _SFFS_INODE_H */
