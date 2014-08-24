/* This file contains directory entry management and the name lookup hashtable.
 *
 * The entry points into this file are:
 *   init_dentry	initialize the directory entry name lookup hashtable
 *   lookup_dentry	find an inode based on parent directory and name
 *   add_dentry		add an inode as directory entry to a parent directory
 *   del_dentry		delete an inode from its parent directory
 *
 * Created:
 *   April 2009 (D.C. van Moolenbroek)
 */

#include "inc.h"

static LIST_HEAD(hash_head, inode) hash_table[NUM_HASH_SLOTS];

static unsigned int hash_dentry(struct inode *parent, char *name);

/*===========================================================================*
 *				init_dentry				     *
 *===========================================================================*/
void init_dentry(void)
{
/* Initialize the names hashtable.
 */
  int i;

  for (i = 0; i < NUM_HASH_SLOTS; i++)
	LIST_INIT(&hash_table[i]);
}

/*===========================================================================*
 *				lookup_dentry				     *
 *===========================================================================*/
struct inode *lookup_dentry(struct inode *parent, char *name)
{
/* Given a directory inode and a component name, look up the inode associated
 * with that directory entry. Return the inode (with increased reference
 * count) if found, or NULL otherwise.
 */
  struct inode *ino;
  unsigned int slot;

  assert(IS_DIR(parent));

  slot = hash_dentry(parent, name);

  LIST_FOREACH(ino, &hash_table[slot], i_hash) {
	if (compare_name(ino->i_name, name) == TRUE)
		break;
  }

  if (ino == NULL)
	return NULL;

  get_inode(ino);

  return ino;
}

/*===========================================================================*
 *				add_dentry				     *
 *===========================================================================*/
void add_dentry(struct inode *parent, char *name, struct inode *ino)
{
/* Add an entry to a parent inode, in the form of a new inode, with the given
 * name. An entry with this name must not already exist.
 */
  unsigned int slot;

  assert(IS_DIR(parent));
  assert(parent->i_ref > 0);
  assert(ino->i_ref > 0);
  assert(name[0]);
  assert(strlen(name) <= NAME_MAX);

  link_inode(parent, ino);

  strlcpy(ino->i_name, name, sizeof(ino->i_name));

  /* hash_add(ino); */
  slot = hash_dentry(parent, ino->i_name);
  LIST_INSERT_HEAD(&hash_table[slot], ino, i_hash);
}

/*===========================================================================*
 *				del_one_dentry				     *
 *===========================================================================*/
static void del_one_dentry(struct inode *ino)
{
/* This inode has become inaccessible by name. Disassociate it from its parent
 * and remove it from the names hash table.
 */

  /* There can and must be exactly one root inode, so don't delete it! */
  if (IS_ROOT(ino))
	return;

  /* INUSE -> DELETED, CACHED -> FREE */

  /* Remove the entry from the hashtable.
   * Decrease parent's refcount, possibly adding it to the free list.
   * Do not touch open handles. Do not add to the free list.
   */

  assert(ino->i_parent != NULL);

  /* hash_del(ino); */
  LIST_REMOVE(ino, i_hash);

  ino->i_name[0] = 0;

  unlink_inode(ino);
}

/*===========================================================================*
 *				del_dentry				     *
 *===========================================================================*/
void del_dentry(struct inode *ino)
{
/* Disassociate an inode from its parent, effectively deleting it. Recursively
 * delete all its children as well, fragmenting the deleted branch into single
 * inodes.
 */
  LIST_HEAD(work_list, inode) work_list;
  struct inode *child;

  del_one_dentry(ino);

  /* Quick way out: one directory entry that itself has no children. */
  if (!HAS_CHILDREN(ino))
	return;

  /* Recursively delete all children of the inode as well.
   * Iterative version: this is potentially 128 levels deep.
   */

  LIST_INIT(&work_list);
  LIST_INSERT_HEAD(&work_list, ino, i_next);

  do {
	ino = LIST_FIRST(&work_list);
	LIST_REMOVE(ino, i_next);

	assert(IS_DIR(ino));

	while (!LIST_EMPTY(&ino->i_child)) {
		child = LIST_FIRST(&ino->i_child);
		LIST_REMOVE(child, i_next);

		del_one_dentry(child);

		if (HAS_CHILDREN(child))
			LIST_INSERT_HEAD(&work_list, child, i_next);
	}
  } while (!LIST_EMPTY(&work_list));
}

/*===========================================================================*
 *				hash_dentry				     *
 *===========================================================================*/
static unsigned int hash_dentry(struct inode *parent, char *name)
{
/* Generate a hash value for a given name. Normalize the name first, so that
 * different variations of the name will result in the same hash value.
 */
  unsigned int val;
  char buf[NAME_MAX+1], *p;

  dprintf(("%s: hash_dentry for '%s'\n", sffs_name, name));

  normalize_name(buf, name);

  /* djb2 string hash algorithm, XOR variant */
  val = 5381;
  for (p = buf; *p; p++)
	val = ((val << 5) + val) ^ *p;

  /* Mix with inode number: typically, many file names occur in several
   * different directories.
   */
  return (val ^ parent->i_num) % NUM_HASH_SLOTS;
}
