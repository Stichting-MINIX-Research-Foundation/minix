/* This file provides path-to-inode lookup functionality.
 *
 * The entry points into this file are:
 *   do_lookup		perform the LOOKUP file system call
 *
 * Created:
 *   April 2009 (D.C. van Moolenbroek)
 */

#include "inc.h"

/*===========================================================================*
 *				go_up					     *
 *===========================================================================*/
static int go_up(
	char path[PATH_MAX],    /* path to take the last part from */
	struct inode *ino,      /* inode of the current directory */
	struct inode **res_ino, /* place to store resulting inode */
	struct sffs_attr *attr  /* place to store inode attributes */
)
{
/* Given an inode, progress into the parent directory.
 */
  struct inode *parent;
  int r;

  pop_path(path);

  parent = ino->i_parent;
  assert(parent != NULL);

  if ((r = verify_path(path, parent, attr, NULL)) != OK)
	return r;

  get_inode(parent);

  *res_ino = parent;

  return r;
}

/*===========================================================================*
 *				go_down					     *
 *===========================================================================*/
static int go_down(
	char path[PATH_MAX],    /* path to add the name to */
	struct inode *parent,   /* inode of the current directory */
	char *name,             /* name of the directory entry */
	struct inode **res_ino, /* place to store resulting inode */
	struct sffs_attr *attr  /* place to store inode attributes */
)
{
/* Given a directory inode and a name, progress into a directory entry.
 */
  struct inode *ino;
  int r, stale = 0;

  if ((r = push_path(path, name)) != OK)
	return r;

  dprintf(("%s: go_down: name '%s', path now '%s'\n", sffs_name, name, path));

  ino = lookup_dentry(parent, name);

  dprintf(("%s: lookup_dentry('%s') returned %p\n", sffs_name, name, ino));

  if (ino != NULL)
	r = verify_path(path, ino, attr, &stale);
  else
	r = sffs_table->t_getattr(path, attr);

  dprintf(("%s: path query returned %d\n", sffs_name, r));

  if (r != OK) {
	if (ino != NULL) {
		put_inode(ino);

		ino = NULL;
	}

	if (!stale)
		return r;
  }

  dprintf(("%s: name '%s'\n", sffs_name, name));

  if (ino == NULL) {
	if ((ino = get_free_inode()) == NULL)
		return ENFILE;

	dprintf(("%s: inode %p ref %d\n", sffs_name, ino, ino->i_ref));

	ino->i_flags = MODE_TO_DIRFLAG(attr->a_mode);

	add_dentry(parent, name, ino);
  }

  *res_ino = ino;
  return OK;
}

/*===========================================================================*
 *				do_lookup				     *
 *===========================================================================*/
int do_lookup(ino_t dir_nr, char *name, struct fsdriver_node *node,
	int *is_mountpt)
{
/* Resolve a path string to an inode.
 */
  struct inode *dir_ino, *ino = NULL;
  struct sffs_attr attr;
  char path[PATH_MAX];
  int r;

  dprintf(("%s: lookup: got query for %"PRIu64", '%s'\n",
	sffs_name, dir_nr, name));

  if ((dir_ino = find_inode(dir_nr)) == NULL)
	return EINVAL;

  attr.a_mask = SFFS_ATTR_MODE | SFFS_ATTR_SIZE;

  if ((r = verify_inode(dir_ino, path, &attr)) != OK)
	return r;

  if (!IS_DIR(dir_ino))
	return ENOTDIR;

  r = OK;
  if (!strcmp(name, "."))
	get_inode(ino = dir_ino);
  else if (!strcmp(name, ".."))
	r = go_up(path, dir_ino, &ino, &attr);
  else
	r = go_down(path, dir_ino, name, &ino, &attr);

  if (r != OK)
	return r;

  node->fn_ino_nr = INODE_NR(ino);
  node->fn_mode = get_mode(ino, attr.a_mode);
  node->fn_size = attr.a_size;
  node->fn_uid = sffs_params->p_uid;
  node->fn_gid = sffs_params->p_gid;
  node->fn_dev = NO_DEV;

  *is_mountpt = FALSE;

  return OK;
}
