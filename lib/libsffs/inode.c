/* This file deals with inode management.
 *
 * The entry points into this file are:
 *   init_inode		initialize the inode table, return the root inode
 *   find_inode		find an inode based on its inode number
 *   get_inode		increase the reference count of an inode
 *   put_inode		decrease the reference count of an inode
 *   link_inode		link an inode as a directory entry to another inode
 *   unlink_inode	unlink an inode from its parent directory
 *   get_free_inode	return a free inode object
 *   have_free_inode	check whether there is a free inode available
 *   have_used_inode	check whether any inode is still in use
 *   do_putnode		perform the PUTNODE file system call
 *
 * Created:
 *   April 2009 (D.C. van Moolenbroek)
 */

#include "inc.h"

static struct inode inodes[NUM_INODES];

static TAILQ_HEAD(free_head, inode) free_list;

/*===========================================================================*
 *				init_inode				     *
 *===========================================================================*/
struct inode *init_inode()
{
/* Initialize inode table. Return the root inode.
 */
  struct inode *ino;
  unsigned int index;

  TAILQ_INIT(&free_list);

  dprintf(("%s: %d inodes, %u bytes each, equals %u bytes\n",
	sffs_name, NUM_INODES, sizeof(struct inode), sizeof(inodes)));

  /* Mark all inodes except the root inode as free. */
  for (index = 1; index < NUM_INODES; index++) {
	ino = &inodes[index];
	ino->i_parent = NULL;
	LIST_INIT(&ino->i_child);
	ino->i_num = index + 1;
	ino->i_gen = (unsigned short)-1; /* aesthetics */
	ino->i_ref = 0;
	ino->i_flags = 0;
	TAILQ_INSERT_TAIL(&free_list, ino, i_free);
  }

  /* Initialize and return the root inode. */
  ino = &inodes[0];
  ino->i_parent = ino;		/* root inode is its own parent */
  LIST_INIT(&ino->i_child);
  ino->i_num = ROOT_INODE_NR;
  ino->i_gen = 0;		/* unused by root node */
  ino->i_ref = 1;		/* root inode is hereby in use */
  ino->i_flags = I_DIR;		/* root inode is a directory */
  ino->i_name[0] = 0;		/* root inode has empty name */

  return ino;
}

/*===========================================================================*
 *				find_inode				     *
 *===========================================================================*/
struct inode *find_inode(ino_nr)
ino_t ino_nr;
{
/* Get an inode based on its inode number. Do not increase its reference count.
 */
  struct inode *ino;
  int index;

  /* Inode 0 (= index -1) is not a valid inode number. */
  index = INODE_INDEX(ino_nr);
  if (index < 0) {
	printf("%s: VFS passed invalid inode number!\n", sffs_name);

	return NULL;
  }

  assert(index < NUM_INODES);

  ino = &inodes[index];

  /* Make sure the generation number matches. */
  if (INODE_GEN(ino_nr) != ino->i_gen) {
	printf("%s: VFS passed outdated inode number!\n", sffs_name);

	return NULL;
  }

  /* The VFS/FS protocol only uses referenced inodes. */
  if (ino->i_ref == 0)
	printf("%s: VFS passed unused inode!\n", sffs_name);

  return ino;
}

/*===========================================================================*
 *				get_inode				     *
 *===========================================================================*/
void get_inode(ino)
struct inode *ino;
{
/* Increase the given inode's reference count. If both reference and link
 * count were zero before, remove the inode from the free list.
 */

  dprintf(("%s: get_inode(%p) ['%s']\n", sffs_name, ino, ino->i_name));

  /* (INUSE, CACHED) -> INUSE */

  /* If this is the first reference, remove the node from the free list. */
  if (ino->i_ref == 0 && !HAS_CHILDREN(ino))
	TAILQ_REMOVE(&free_list, ino, i_free);

  ino->i_ref++;

  if (ino->i_ref == 0)
	panic("inode reference count wrapped");
}

/*===========================================================================*
 *				put_inode				     *
 *===========================================================================*/
void put_inode(ino)
struct inode *ino;
{
/* Decrease an inode's reference count. If this count has reached zero, close
 * the inode's file handle, if any. If both reference and link count have
 * reached zero, mark the inode as cached or free.
 */

  dprintf(("%s: put_inode(%p) ['%s']\n", sffs_name, ino, ino->i_name));

  assert(ino != NULL);
  assert(ino->i_ref > 0);

  ino->i_ref--;

  /* If there are still references to this inode, we're done here. */
  if (ino->i_ref > 0)
	return;

  /* Close any file handle associated with this inode. */
  put_handle(ino);

  /* Only add the inode to the free list if there are also no links to it. */
  if (HAS_CHILDREN(ino))
	return;

  /* INUSE -> CACHED, DELETED -> FREE */

  /* Add the inode to the head or tail of the free list, depending on whether
   * it is also deleted (and therefore can never be reused as is).
   */
  if (ino->i_parent == NULL)
	TAILQ_INSERT_HEAD(&free_list, ino, i_free);
  else
	TAILQ_INSERT_TAIL(&free_list, ino, i_free);
}

/*===========================================================================*
 *				link_inode				     *
 *===========================================================================*/
void link_inode(parent, ino)
struct inode *parent;
struct inode *ino;
{
/* Link an inode to a parent. If both reference and link count were zero
 * before, remove the inode from the free list. This function should only be
 * called from add_dentry().
 */

  /* This can never happen, right? */
  if (parent->i_ref == 0 && !HAS_CHILDREN(parent))
	TAILQ_REMOVE(&free_list, parent, i_free);

  LIST_INSERT_HEAD(&parent->i_child, ino, i_next);

  ino->i_parent = parent;
}

/*===========================================================================*
 *				unlink_inode				     *
 *===========================================================================*/
void unlink_inode(ino)
struct inode *ino;
{
/* Unlink an inode from its parent. If both reference and link count have
 * reached zero, mark the inode as cached or free. This function should only
 * be used from del_dentry().
 */
  struct inode *parent;

  parent = ino->i_parent;

  LIST_REMOVE(ino, i_next);

  if (parent->i_ref == 0 && !HAS_CHILDREN(parent)) {
	if (parent->i_parent == NULL)
		TAILQ_INSERT_HEAD(&free_list, parent, i_free);
	else
		TAILQ_INSERT_TAIL(&free_list, parent, i_free);
  }

  ino->i_parent = NULL;
}

/*===========================================================================*
 *				get_free_inode				     *
 *===========================================================================*/
struct inode *get_free_inode()
{
/* Return a free inode object (with reference count 1), if available.
 */
  struct inode *ino;

  /* [CACHED -> FREE,] FREE -> DELETED */

  /* If there are no inodes on the free list, we cannot satisfy the request. */
  if (TAILQ_EMPTY(&free_list)) {
	printf("%s: out of inodes!\n", sffs_name);

	return NULL;
  }

  ino = TAILQ_FIRST(&free_list);
  TAILQ_REMOVE(&free_list, ino, i_free);

  assert(ino->i_ref == 0);
  assert(!HAS_CHILDREN(ino));

  /* If this was a cached inode, free it first. */
  if (ino->i_parent != NULL)
	del_dentry(ino);

  assert(ino->i_parent == NULL);

  /* Initialize a subset of its fields */
  ino->i_gen++;
  ino->i_ref = 1;

  return ino;
}

/*===========================================================================*
 *				have_free_inode				     *
 *===========================================================================*/
int have_free_inode()
{
/* Check whether there are any free inodes at the moment. Kind of lame, but
 * this allows for easier error recovery in some places.
 */

  return !TAILQ_EMPTY(&free_list);
}

/*===========================================================================*
 *				have_used_inode				     *
 *===========================================================================*/
int have_used_inode()
{
/* Check whether any inodes are still in use, that is, any of the inodes have
 * a reference count larger than zero.
 */
  unsigned int index;

  for (index = 0; index < NUM_INODES; index++)
	if (inodes[index].i_ref > 0)
		return TRUE;

  return FALSE;
}

/*===========================================================================*
 *				do_putnode				     *
 *===========================================================================*/
int do_putnode()
{
/* Decrease an inode's reference count.
 */
  struct inode *ino;
  int count;

  if ((ino = find_inode(m_in.REQ_INODE_NR)) == NULL)
	return EINVAL;

  count = m_in.REQ_COUNT;

  if (count <= 0 || count > ino->i_ref) return EINVAL;

  ino->i_ref -= count - 1;

  put_inode(ino);

  return OK;
}
