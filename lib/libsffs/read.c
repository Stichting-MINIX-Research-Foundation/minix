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

#define DWORD_ALIGN(len) (((len) + sizeof(long) - 1) & ~(sizeof(long) - 1))

/*===========================================================================*
 *				do_read					     *
 *===========================================================================*/
int do_read()
{
/* Read data from a file.
 */
  struct inode *ino;
  u64_t pos;
  size_t count, size;
  vir_bytes off;
  char *ptr;
  int r, chunk;

  if ((ino = find_inode(m_in.REQ_INODE_NR)) == NULL)
	return EINVAL;

  if (IS_DIR(ino)) return EISDIR;

  if ((r = get_handle(ino)) != OK)
	return r;

  pos = make64(m_in.REQ_SEEK_POS_LO, m_in.REQ_SEEK_POS_HI);
  count = m_in.REQ_NBYTES;

  assert(count > 0);

  /* Use the buffer from below to eliminate extra copying. */
  size = sffs_table->t_readbuf(&ptr);
  off = 0;

  while (count > 0) {
	chunk = MIN(count, size);

	if ((r = sffs_table->t_read(ino->i_file, ptr, chunk, pos)) <= 0)
		break;

	chunk = r;

	r = sys_safecopyto(m_in.m_source, m_in.REQ_GRANT, off,
		(vir_bytes) ptr, chunk);

	if (r != OK)
		break;

	count -= chunk;
	off += chunk;
	pos = add64u(pos, chunk);
  }

  if (r < 0)
	return r;

  m_out.RES_SEEK_POS_HI = ex64hi(pos);
  m_out.RES_SEEK_POS_LO = ex64lo(pos);
  m_out.RES_NBYTES = off;

  return OK;
}

/*===========================================================================*
 *				do_getdents				     *
 *===========================================================================*/
int do_getdents()
{
/* Retrieve directory entries.
 */
  char name[NAME_MAX+1];
  struct inode *ino, *child;
  struct dirent *dent;
  struct sffs_attr attr;
  size_t len, off, user_off, user_left;
  off_t pos;
  int r;
  /* must be at least sizeof(struct dirent) + NAME_MAX */
  static char buf[BLOCK_SIZE];

  attr.a_mask = SFFS_ATTR_MODE;

  if ((ino = find_inode(m_in.REQ_INODE_NR)) == NULL)
	return EINVAL;

  if (m_in.REQ_SEEK_POS_HI != 0) return EINVAL;

  if (!IS_DIR(ino)) return ENOTDIR;

  /* We are going to need at least one free inode to store children in. */
  if (!have_free_inode()) return ENFILE;

  /* If we don't have a directory handle yet, get one now. */
  if ((r = get_handle(ino)) != OK)
	return r;

  off = 0;
  user_off = 0;
  user_left = m_in.REQ_MEM_SIZE;

  /* We use the seek position as file index number. The first position is for
   * the "." entry, the second position is for the ".." entry, and the next
   * position numbers each represent a file in the directory.
   */
  for (pos = m_in.REQ_SEEK_POS_LO; ; pos++) {
	/* Determine which inode and name to use for this entry.
	 * We have no idea whether the host will give us "." and/or "..",
	 * so generate our own and skip those from the host.
	 */
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

	len = DWORD_ALIGN(sizeof(struct dirent) + strlen(name));

	/* Is the user buffer too small to store another record?
	 * Note that we will be rerequesting the same dentry upon a subsequent
	 * getdents call this way, but we really need the name length for this.
	 */
	if (user_off + off + len > user_left) {
		put_inode(child);

		/* Is the user buffer too small for even a single record? */
		if (user_off == 0 && off == 0)
			return EINVAL;

		break;
	}

	/* If our own buffer cannot contain the new record, copy out first. */
	if (off + len > sizeof(buf)) {
		r = sys_safecopyto(m_in.m_source, m_in.REQ_GRANT,
			user_off, (vir_bytes) buf, off);

		if (r != OK) {
			put_inode(child);

			return r;
		}

		user_off += off;
		user_left -= off;
		off = 0;
	}

	/* Fill in the actual directory entry. */
	dent = (struct dirent *) &buf[off];
	dent->d_ino = INODE_NR(child);
	dent->d_off = pos;
	dent->d_reclen = len;
	strcpy(dent->d_name, name);

	off += len;

	put_inode(child);
  }

  /* If there is anything left in our own buffer, copy that out now. */
  if (off > 0) {
	r = sys_safecopyto(m_in.m_source, m_in.REQ_GRANT, user_off,
		(vir_bytes) buf, off);

	if (r != OK)
		return r;

	user_off += off;
  }

  m_out.RES_SEEK_POS_HI = 0;
  m_out.RES_SEEK_POS_LO = pos;
  m_out.RES_NBYTES = user_off;

  return OK;
}
