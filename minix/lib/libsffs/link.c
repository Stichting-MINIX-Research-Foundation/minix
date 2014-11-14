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

/*===========================================================================*
 *				do_create				     *
 *===========================================================================*/
int do_create(ino_t dir_nr, char *name, mode_t mode, uid_t uid, gid_t gid,
	struct fsdriver_node *node)
{
/* Create a new file.
 */
  char path[PATH_MAX];
  struct inode *parent, *ino;
  struct sffs_attr attr;
  sffs_file_t handle;
  int r;

  /* We cannot create files on a read-only file system. */
  if (read_only)
	return EROFS;

  if ((parent = find_inode(dir_nr)) == NULL)
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
  r = sffs_table->t_open(path, O_CREAT | O_EXCL | O_RDWR, mode, &handle);

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

  node->fn_ino_nr = INODE_NR(ino);
  node->fn_mode = get_mode(ino, attr.a_mode);
  node->fn_size = attr.a_size;
  node->fn_uid = sffs_params->p_uid;
  node->fn_gid = sffs_params->p_gid;
  node->fn_dev = NO_DEV;

  return OK;
}

/*===========================================================================*
 *				do_mkdir				     *
 *===========================================================================*/
int do_mkdir(ino_t dir_nr, char *name, mode_t mode, uid_t uid, gid_t gid)
{
/* Make a new directory.
 */
  char path[PATH_MAX];
  struct inode *parent, *ino;
  int r;

  /* We cannot create directories on a read-only file system. */
  if (read_only)
	return EROFS;

  if ((parent = find_inode(dir_nr)) == NULL)
	return EINVAL;

  if ((r = verify_dentry(parent, name, path, &ino)) != OK)
	return r;

  /* Perform the actual mkdir call. */
  r = sffs_table->t_mkdir(path, mode);

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
int do_unlink(ino_t dir_nr, char *name, int call)
{
/* Delete a file.
 */
  char path[PATH_MAX];
  struct inode *parent, *ino;
  int r;

  /* We cannot delete files on a read-only file system. */
  if (read_only)
	return EROFS;

  if ((parent = find_inode(dir_nr)) == NULL)
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
int do_rmdir(ino_t dir_nr, char *name, int call)
{
/* Remove an empty directory.
 */
  char path[PATH_MAX];
  struct inode *parent, *ino;
  int r;

  /* We cannot remove directories on a read-only file system. */
  if (read_only)
	return EROFS;

  if ((parent = find_inode(dir_nr)) == NULL)
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
int do_rename(ino_t old_dir_nr, char *old_name, ino_t new_dir_nr,
	char *new_name)
{
/* Rename a file or directory.
 */
  char old_path[PATH_MAX], new_path[PATH_MAX];
  struct inode *old_parent, *new_parent;
  struct inode *old_ino, *new_ino;
  int r;

  /* We cannot do rename on a read-only file system. */
  if (read_only)
	return EROFS;

  /* Get possibly preexisting inodes for the old and new paths. */
  if ((old_parent = find_inode(old_dir_nr)) == NULL ||
	(new_parent = find_inode(new_dir_nr)) == NULL)
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
