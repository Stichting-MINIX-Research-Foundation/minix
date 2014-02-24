/* This file contains routines for creating and manipulating path strings.
 *
 * The entry points into this file are:
 *   make_path		construct a path string for an inode
 *   push_path		add a path component to the end of a path string
 *   pop_path		remove the last path component from a path string
 *
 * Created:
 *   April 2009 (D.C. van Moolenbroek)
 */

#include "inc.h"

/*===========================================================================*
 *				make_path				     *
 *===========================================================================*/
int make_path(char path[PATH_MAX], struct inode *ino)
{
/* Given an inode, construct the path identifying that inode.
 */
  char buf[PATH_MAX], *p, *prefix;
  size_t len, plen, total;

  p = &buf[sizeof(buf) - 1];
  p[0] = 0;

  dprintf(("%s: make_path: constructing path for inode %d\n",
	sffs_name, ino->i_num));

  /* Get the length of the prefix, skipping any leading slashes. */
  for (prefix = sffs_params->p_prefix; prefix[0] == '/'; prefix++);
  plen = strlen(prefix);

  /* Construct the path right-to-left in a temporary buffer first. */
  for (total = plen; ino != NULL && !IS_ROOT(ino); ino = ino->i_parent) {
	len = strlen(ino->i_name);

	total += len + 1;
	p -= len + 1;

	if (total >= sizeof(buf))
		return ENAMETOOLONG;

	p[0] = '/';
	memcpy(p + 1, ino->i_name, len);
  }

  /* If any of the intermediate inodes has no parent, the final inode is no
   * longer addressable by name.
   */
  if (ino == NULL)
	return ENOENT;

  /* Put the result in the actual buffer. We need the leading slash in the
   * temporary buffer only when the prefix is not empty.
   */
  if (!prefix[0] && p[0] == '/') p++;

  strlcpy(path, prefix, PATH_MAX);
  strlcpy(&path[plen], p, PATH_MAX - plen);

  dprintf(("%s: make_path: resulting path is '%s'\n", sffs_name, path));

  return OK;
}

/*===========================================================================*
 *				push_path				     *
 *===========================================================================*/
int push_path(char path[PATH_MAX], char *name)
{
/* Add a component to the end of a path.
 */
  size_t len, add;

  len = strlen(path);
  add = strlen(name);
  if (len > 0) add++;

  if (len + add >= PATH_MAX)
	return ENAMETOOLONG;

  if (len > 0) path[len++] = '/';
  strlcpy(&path[len], name, PATH_MAX - len);

  return OK;
}

/*===========================================================================*
 *				pop_path				     *
 *===========================================================================*/
void pop_path(char path[PATH_MAX])
{
/* Remove the last component from a path.
 */
  char *p;

  p = strrchr(path, '/');

  if (p == NULL) {
	p = path;

	/* Can't pop the root component */
	assert(p[0] != 0);
  }

  p[0] = 0;
}
