/* This file contains file and directory reading file system call handlers.
 *
 * The entry points into this file are:
 *   do_read		perform the READ file system call
 *   do_getdents	perform the GETDENTS file system call
 *
 * Created:
 *   April 2009 (D.C. van Moolenbroek)
 */

#include "inc.h"

#include <dirent.h>

/*===========================================================================*
 *				do_read					     *
 *===========================================================================*/
ssize_t do_read(ino_t ino_nr, struct fsdriver_data *data, size_t count,
	off_t pos, int call)
{
/* Read data from a file.
 */
  struct inode *ino;
  size_t size, off;
  char *ptr;
  int r, chunk;

  if ((ino = find_inode(ino_nr)) == NULL)
	return EINVAL;

  if (IS_DIR(ino)) return EISDIR;

  if ((r = get_handle(ino)) != OK)
	return r;

  assert(count > 0);

  /* Use the buffer from below to eliminate extra copying. */
  size = sffs_table->t_readbuf(&ptr);
  off = 0;

  while (count > 0) {
	chunk = MIN(count, size);

	if ((r = sffs_table->t_read(ino->i_file, ptr, chunk, pos)) <= 0)
		break;

	chunk = r;

	if ((r = fsdriver_copyout(data, off, ptr, chunk)) != OK)
		break;

	count -= chunk;
	off += chunk;
	pos += chunk;
  }

  if (r < 0)
	return r;

  return off;
}

/*===========================================================================*
 *				do_getdents				     *
 *===========================================================================*/
ssize_t do_getdents(ino_t ino_nr, struct fsdriver_data *data, size_t bytes,
	off_t *posp)
{
/* Retrieve directory entries.
 */
  struct fsdriver_dentry fsdentry;
  char name[NAME_MAX+1];
  struct inode *ino, *child;
  struct sffs_attr attr;
  off_t pos;
  int r;
  /* must be at least sizeof(struct dirent) + NAME_MAX */
  static char buf[BLOCK_SIZE];

  if ((ino = find_inode(ino_nr)) == NULL)
	return EINVAL;

  if (!IS_DIR(ino)) return ENOTDIR;

  if (*posp < 0 || *posp >= ULONG_MAX) return EINVAL;

  /* We are going to need at least one free inode to store children in. */
  if (!have_free_inode()) return ENFILE;

  /* If we don't have a directory handle yet, get one now. */
  if ((r = get_handle(ino)) != OK)
	return r;

  fsdriver_dentry_init(&fsdentry, data, bytes, buf, sizeof(buf));

  /* We use the seek position as file index number. The first position is for
   * the "." entry, the second position is for the ".." entry, and the next
   * position numbers each represent a file in the directory.
   */
  for (;;) {
	/* Determine which inode and name to use for this entry.
	 * We have no idea whether the host will give us "." and/or "..",
	 * so generate our own and skip those from the host.
	 */
	pos = (*posp)++;

	if (pos == 0) {
		/* Entry for ".". */
		child = ino;

		strcpy(name, ".");

		get_inode(child);
	}
	else if (pos == 1) {
		/* Entry for "..", but only when there is a parent. */
		if (ino->i_parent == NULL)
			continue;

		child = ino->i_parent;

		strcpy(name, "..");

		get_inode(child);
	}
	else {
		/* Any other entry, not being "." or "..". */
		attr.a_mask = SFFS_ATTR_MODE;

		r = sffs_table->t_readdir(ino->i_dir, pos - 2, name,
			sizeof(name), &attr);

		if (r != OK) {
			/* No more entries? Then close the handle and stop. */
			if (r == ENOENT) {
				put_handle(ino);

				break;
			}

			/* FIXME: what if the error is ENAMETOOLONG? */
			return r;
		}

		if (!strcmp(name, ".") || !strcmp(name, ".."))
			continue;

		if ((child = lookup_dentry(ino, name)) == NULL) {
			child = get_free_inode();

			/* We were promised a free inode! */
			assert(child != NULL);

			child->i_flags = MODE_TO_DIRFLAG(attr.a_mode);

			add_dentry(ino, name, child);
		}
	}

	r = fsdriver_dentry_add(&fsdentry, INODE_NR(child), name, strlen(name),
		IS_DIR(child) ? DT_DIR : DT_REG);

	put_inode(child);

	if (r < 0)
		return r;
	if (r == 0)
		break;
  }

  return fsdriver_dentry_finish(&fsdentry);
}
