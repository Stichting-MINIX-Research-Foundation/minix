/* This file contains open file and directory handle management functions.
 *
 * The entry points into this file are:
 *   get_handle		open a handle for an inode and store the handle
 *   put_handle		close any handles associated with an inode
 *
 * Created:
 *   April 2009 (D.C. van Moolenbroek)
 */

#include "inc.h"

#include <fcntl.h>

/*===========================================================================*
 *				get_handle				     *
 *===========================================================================*/
int get_handle(struct inode *ino)
{
/* Get an open file or directory handle for an inode.
 */
  char path[PATH_MAX];
  int r;

  /* If we don't have a file handle yet, try to open the file now. */
  if (ino->i_flags & I_HANDLE)
	return OK;

  if ((r = verify_inode(ino, path, NULL)) != OK)
	return r;

  if (IS_DIR(ino)) {
	r = sffs_table->t_opendir(path, &ino->i_dir);
  }
  else {
	if (!read_only)
		r = sffs_table->t_open(path, O_RDWR, 0, &ino->i_file);

	/* Protection or mount status might prevent us from writing. With the
	 * information that we have available, this is the best we can do..
	 */
	if (read_only || r != OK)
		r = sffs_table->t_open(path, O_RDONLY, 0, &ino->i_file);
  }

  if (r != OK)
	return r;

  ino->i_flags |= I_HANDLE;

  return OK;
}

/*===========================================================================*
 *				put_handle				     *
 *===========================================================================*/
void put_handle(struct inode *ino)
{
/* Close an open file or directory handle associated with an inode.
 */
  int r;

  if (!(ino->i_flags & I_HANDLE))
	return;

  /* We ignore any errors here, because we can't deal with them anyway. */
  if (IS_DIR(ino))
	r = sffs_table->t_closedir(ino->i_dir);
  else
	r = sffs_table->t_close(ino->i_file);

  if (r != OK)
	printf("%s: put_handle: handle close operation returned %d\n",
		sffs_name, r);

  ino->i_flags &= ~I_HANDLE;
}
