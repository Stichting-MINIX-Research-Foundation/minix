/* Part of libhgfs - (c) 2009, D.C. van Moolenbroek */

#include "inc.h"

/*===========================================================================*
 *				hgfs_opendir				     *
 *===========================================================================*/
int hgfs_opendir(char *path, sffs_dir_t *handle)
{
/* Open a directory. Store a directory handle upon success.
 */
  int r;

  RPC_REQUEST(HGFS_REQ_OPENDIR);

  path_put(path);

  if ((r = rpc_query()) != OK)
	return r;

  *handle = (sffs_dir_t)RPC_NEXT32;

  return OK;
}

/*===========================================================================*
 *				hgfs_readdir				     *
 *===========================================================================*/
int hgfs_readdir(sffs_dir_t handle, unsigned int index, char *buf,
	size_t size, struct sffs_attr *attr)
{
/* Read a directory entry from an open directory, using a zero-based index
 * number. Upon success, the resulting path name is stored in the given buffer
 * and the given attribute structure is filled selectively as requested. Upon
 * error, the contents of the path buffer and attribute structure are
 * undefined. ENOENT is returned upon end of directory.
 */
  int r;

  RPC_REQUEST(HGFS_REQ_READDIR);
  RPC_NEXT32 = (u32_t)handle;
  RPC_NEXT32 = index;

  /* EINVAL signifies end of directory. */
  if ((r = rpc_query()) != OK)
	return (r == EINVAL) ? ENOENT : OK;

  attr_get(attr);

  if ((r = path_get(buf, size)) != OK)
	return r;

  /* VMware Player 3 returns an empty name, instead of EINVAL, when reading
   * from an EOF position right after opening the directory handle. Seems to be
   * a newly introduced bug..
   */
  return (!buf[0]) ? ENOENT : OK;
}

/*===========================================================================*
 *				hgfs_closedir				     *
 *===========================================================================*/
int hgfs_closedir(sffs_dir_t handle)
{
/* Close an open directory.
 */

  RPC_REQUEST(HGFS_REQ_CLOSEDIR);
  RPC_NEXT32 = (u32_t)handle;

  return rpc_query();
}
