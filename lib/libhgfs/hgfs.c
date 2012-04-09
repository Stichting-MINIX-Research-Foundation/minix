/* Part of libhgfs - (c) 2009, D.C. van Moolenbroek */

#include "inc.h"

struct sffs_table hgfs_table = {
  .t_open	= hgfs_open,
  .t_read	= hgfs_read,
  .t_write	= hgfs_write,
  .t_close	= hgfs_close,

  .t_readbuf	= hgfs_readbuf,
  .t_writebuf	= hgfs_writebuf,

  .t_opendir	= hgfs_opendir,
  .t_readdir	= hgfs_readdir,
  .t_closedir	= hgfs_closedir,

  .t_getattr	= hgfs_getattr,
  .t_setattr	= hgfs_setattr,

  .t_mkdir	= hgfs_mkdir,
  .t_unlink	= hgfs_unlink,
  .t_rmdir	= hgfs_rmdir,
  .t_rename	= hgfs_rename,

  .t_queryvol	= hgfs_queryvol,
};

/*===========================================================================*
 *				hgfs_init				     *
 *===========================================================================*/
int hgfs_init(const struct sffs_table **tablep)
{
/* Initialize the library. Return OK on success, or a negative error code
 * otherwise. If EAGAIN is returned, shared folders are disabled.
 */
  int r;

  time_init();

  r = rpc_open();

  if (r == OK)
	*tablep = &hgfs_table;

  return r;
}

/*===========================================================================*
 *				hgfs_cleanup				     *
 *===========================================================================*/
void hgfs_cleanup()
{
/* Clean up state.
 */

  rpc_close();
}

