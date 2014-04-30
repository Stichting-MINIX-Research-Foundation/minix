/* This file contains directory entry related file system call handlers.
 *
 * The entry points into this file are:
 *   do_create		perform the CREATE file system call
 *   do_mkdir		perform the MKDIR file system call
 *   do_unlink		perform the UNLINK file system call
 *   do_rmdir		perform the RMDIR file system call
 *   do_rename		perform the RENAME file system call
 *
 * Created:
 *   April 2009 (D.C. van Moolenbroek)
 */

#include "inc.h"

#include <fcntl.h>

static int force_remove(char *path, int dir);

/*===========================================================================*
 *				do_create				     *
 *===========================================================================*/
int do_create(void)
{
/* Create a new file.
 */
  char path[PATH_MAX], name[NAME_MAX+1];
  struct inode *parent, *ino;
  struct sffs_attr attr;
  sffs_file_t handle;
  int r;

  /* We cannot create files on a read-only file system. */
  if (state.s_read_only)
	return EROFS;

  /* Get path, name, parent inode and possibly inode for the given path. */
  if ((r = get_name(m_in.m_vfs_fs_create.grant, m_in.m_vfs_fs_create.path_len, name)) != OK)
	return r;

  if (!strcmp(name, ".") || !strcmp(name, "..")) return EEXIST;

  if ((parent = find_inode(m_in.m_vfs_fs_create.inode)) == NULL)
	return EINVAL;

  if ((r = verify_dentry(parent, name, path, &ino)) != OK)
	return r;

  /* Are we going to need a new inode upon success?
   * Then make sure there is one available before trying anything.
   */
  if (ino == NULL || ino->i_ref > 1 || HAS_CHILDREN(ino)) {
	if (!have_free_inode()) {
		if (ino != NULL)
			put_inode(ino);

		return ENFILE;
	}
  }

  /* Perform the actual create call. */
  r = sffs_table->t_open(path, O_CREAT | O_EXCL | O_RDWR, m_in.m_vfs_fs_create.mode,
	&handle);

  if (r != OK) {
	/* Let's not try to be too clever with error codes here. If something
	 * is wrong with the directory, we'll find out later anyway.
	 */

	if (ino != NULL)
		put_inode(ino);

	return r;
  }

  /* Get the created file's attributes. */
  attr.a_mask = SFFS_ATTR_MODE | SFFS_ATTR_SIZE;
  r = sffs_table->t_getattr(path, &attr);

  /* If this fails, or returns a directory, we have a problem. This
   * scenario is in fact possible with race conditions.
   * Simulate a close and return a somewhat appropriate error.
   */
  if (r != OK || S_ISDIR(attr.a_mode)) {
	printf("%s: lost file after creation!\n", sffs_name);

	sffs_table->t_close(handle);

	if (ino != NULL) {
		del_dentry(ino);

		put_inode(ino);
	}

	return (r == OK) ? EEXIST : r;
  }

  /* We do assume that the underlying open(O_CREAT|O_EXCL) call did its job.
   * If we previousy found an inode, get rid of it now. It's old.
   */
  if (ino != NULL) {
	del_dentry(ino);

	put_inode(ino);
  }

  /* Associate the open file handle with an inode, and reply with its details.
   */
  ino = get_free_inode();

  assert(ino != NULL); /* we checked before whether we had a free one */

  ino->i_file = handle;
  ino->i_flags = I_HANDLE;

  add_dentry(parent, name, ino);

  m_out.m_fs_vfs_create.inode = INODE_NR(ino);
  m_out.m_fs_vfs_create.mode = get_mode(ino, attr.a_mode);
  m_out.m_fs_vfs_create.file_size = attr.a_size;
  m_out.m_fs_vfs_create.uid = sffs_params->p_uid;
  m_out.m_fs_vfs_create.gid = sffs_params->p_gid;

  return OK;
}

/*===========================================================================*
 *				do_mkdir				     *
 *===========================================================================*/
int do_mkdir(void)
{
/* Make a new directory.
 */
  char path[PATH_MAX], name[NAME_MAX+1];
  struct inode *parent, *ino;
  int r;

  /* We cannot create directories on a read-only file system. */
  if (state.s_read_only)
	return EROFS;

  /* Get the path string and possibly an inode for the given path. */
  if ((r = get_name(m_in.m_vfs_fs_mkdir.grant, m_in.m_vfs_fs_mkdir.path_len, name)) != OK)
	return r;

  if (!strcmp(name, ".") || !strcmp(name, "..")) return EEXIST;

  if ((parent = find_inode(m_in.m_vfs_fs_mkdir.inode)) == NULL)
	return EINVAL;

  if ((r = verify_dentry(parent, name, path, &ino)) != OK)
	return r;

  /* Perform the actual mkdir call. */
  r = sffs_table->t_mkdir(path, m_in.m_vfs_fs_mkdir.mode);

  if (r != OK) {
	if (ino != NULL)
		put_inode(ino);

	return r;
  }

  /* If we thought the new dentry already existed, it was apparently gone
   * already. Delete it.
   */
  if (ino != NULL) {
	del_dentry(ino);

	put_inode(ino);
  }

  return OK;
}

/*===========================================================================*
 *				force_remove				     *
 *===========================================================================*/
static int force_remove(
	char *path,	/* path to file or directory */
	int dir	   	/* TRUE iff directory */
)
{
/* Remove a file or directory. Wrapper around unlink and rmdir that makes the
 * target temporarily writable if the operation fails with an access denied
 * error. On Windows hosts, read-only files or directories cannot be removed
 * (even though they can be renamed). In general, the SFFS library follows the
 * behavior of the host file system, but this case just confuses the hell out
 * of the MINIX userland..
 */
  struct sffs_attr attr;
  int r, r2;

  /* First try to remove the target. */
  if (dir)
	r = sffs_table->t_rmdir(path);
  else
	r = sffs_table->t_unlink(path);

  if (r != EACCES) return r;

  /* If this fails with an access error, retrieve the target's mode. */
  attr.a_mask = SFFS_ATTR_MODE;

  r2 = sffs_table->t_getattr(path, &attr);

  if (r2 != OK || (attr.a_mode & S_IWUSR)) return r;

  /* If the target is not writable, temporarily set it to writable. */
  attr.a_mode |= S_IWUSR;

  r2 = sffs_table->t_setattr(path, &attr);

  if (r2 != OK) return r;

  /* Then try the original operation again. */
  if (dir)
	r = sffs_table->t_rmdir(path);
  else
	r = sffs_table->t_unlink(path);

  if (r == OK) return r;

  /* If the operation still fails, unset the writable bit again. */
  attr.a_mode &= ~S_IWUSR;

  sffs_table->t_setattr(path, &attr);

  return r;
}

/*===========================================================================*
 *				do_unlink				     *
 *===========================================================================*/
int do_unlink(void)
{
/* Delete a file.
 */
  char path[PATH_MAX], name[NAME_MAX+1];
  struct inode *parent, *ino;
  int r;

  /* We cannot delete files on a read-only file system. */
  if (state.s_read_only)
	return EROFS;

  /* Get the path string and possibly preexisting inode for the given path. */
  if ((r = get_name(m_in.m_vfs_fs_unlink.grant, m_in.m_vfs_fs_unlink.path_len, name)) != OK)
	return r;

  if (!strcmp(name, ".") || !strcmp(name, "..")) return EPERM;

  if ((parent = find_inode(m_in.m_vfs_fs_unlink.inode)) == NULL)
	return EINVAL;

  if ((r = verify_dentry(parent, name, path, &ino)) != OK)
	return r;

  /* Perform the unlink call. */
  r = force_remove(path, FALSE /*dir*/);

  if (r != OK) {
	if (ino != NULL)
		put_inode(ino);

	return r;
  }

  /* If a dentry existed for this name, it is gone now. */
  if (ino != NULL) {
	del_dentry(ino);

	put_inode(ino);
  }

  return OK;
}

/*===========================================================================*
 *				do_rmdir				     *
 *===========================================================================*/
int do_rmdir(void)
{
/* Remove an empty directory.
 */
  char path[PATH_MAX], name[NAME_MAX+1];
  struct inode *parent, *ino;
  int r;

  /* We cannot remove directories on a read-only file system. */
  if (state.s_read_only)
	return EROFS;

  /* Get the path string and possibly preexisting inode for the given path. */
  if ((r = get_name(m_in.m_vfs_fs_unlink.grant, m_in.m_vfs_fs_unlink.path_len, name)) != OK)
	return r;

  if (!strcmp(name, ".")) return EINVAL;
  if (!strcmp(name, "..")) return ENOTEMPTY;

  if ((parent = find_inode(m_in.m_vfs_fs_unlink.inode)) == NULL)
	return EINVAL;

  if ((r = verify_dentry(parent, name, path, &ino)) != OK)
	return r;

  /* Perform the rmdir call. */
  r = force_remove(path, TRUE /*dir*/);

  if (r != OK) {
	if (ino != NULL)
		put_inode(ino);

	return r;
  }

  /* If a dentry existed for this name, it is gone now. */
  if (ino != NULL) {
	del_dentry(ino);

	put_inode(ino);
  }

  return OK;
}

/*===========================================================================*
 *				do_rename				     *
 *===========================================================================*/
int do_rename(void)
{
/* Rename a file or directory.
 */
  char old_path[PATH_MAX], new_path[PATH_MAX];
  char old_name[NAME_MAX+1], new_name[NAME_MAX+1];
  struct inode *old_parent, *new_parent;
  struct inode *old_ino, *new_ino;
  int r;

  /* We cannot do rename on a read-only file system. */
  if (state.s_read_only)
	return EROFS;

  /* Get path strings, names, directory inodes and possibly preexisting inodes
   * for the old and new paths.
   */
  if ((r = get_name(m_in.m_vfs_fs_rename.grant_old, m_in.m_vfs_fs_rename.len_old,
	old_name)) != OK) return r;

  if ((r = get_name(m_in.m_vfs_fs_rename.grant_new, m_in.m_vfs_fs_rename.len_new,
	new_name)) != OK) return r;

  if (!strcmp(old_name, ".") || !strcmp(old_name, "..") ||
	!strcmp(new_name, ".") || !strcmp(new_name, "..")) return EINVAL;

  if ((old_parent = find_inode(m_in.m_vfs_fs_rename.dir_old)) == NULL ||
	(new_parent = find_inode(m_in.m_vfs_fs_rename.dir_new)) == NULL)
	return EINVAL;

  if ((r = verify_dentry(old_parent, old_name, old_path, &old_ino)) != OK)
	return r;

  if ((r = verify_dentry(new_parent, new_name, new_path, &new_ino)) != OK) {
	if (old_ino != NULL)
		put_inode(old_ino);

	return r;
  }

  /* Perform the actual rename call. */
  r = sffs_table->t_rename(old_path, new_path);

  /* If we failed, or if we have nothing further to do: both inodes are
   * NULL, or they both refer to the same file.
   */
  if (r != OK || old_ino == new_ino) {
	if (old_ino != NULL) put_inode(old_ino);

	if (new_ino != NULL) put_inode(new_ino);

	return r;
  }

  /* If the new dentry already existed, it has now been overwritten.
   * Delete the associated inode if we had found one.
   */
  if (new_ino != NULL) {
	del_dentry(new_ino);

	put_inode(new_ino);
  }

  /* If the old dentry existed, rename it accordingly. */
  if (old_ino != NULL) {
	del_dentry(old_ino);

	add_dentry(new_parent, new_name, old_ino);

	put_inode(old_ino);
  }

  return OK;
}
