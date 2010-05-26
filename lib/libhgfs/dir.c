/* Part of libhgfs - (c) 2009, D.C. van Moolenbroek */

#include "inc.h"

/*===========================================================================*
 *				hgfs_opendir				     *
 *===========================================================================*/
PUBLIC int hgfs_opendir(path, handle)
char *path;
hgfs_dir_t *handle;
{
/* Open a directory. Store a directory handle upon success.
 */
  int r;

  RPC_REQUEST(HGFS_REQ_OPENDIR);

  path_put(path);

  if ((r = rpc_query()) != OK)
	return r;

  *handle = (hgfs_dir_t)RPC_NEXT32;

  return OK;
}

/*===========================================================================*
 *				hgfs_readdir				     *
 *===========================================================================*/
PUBLIC int hgfs_readdir(handle, index, buf, size, attr)
hgfs_dir_t handle;
unsigned int index;
char *buf;
size_t size;
struct hgfs_attr *attr;
{
/* Read a directory entry from an open directory, using a zero-based index
 * number. Upon success, the resulting path name is stored in the given buffer
 * and the given attribute structure is filled selectively as requested. Upon
 * error, the contents of the path buffer and attribute structure are
 * undefined.
 */
  int r;

  RPC_REQUEST(HGFS_REQ_READDIR);
  RPC_NEXT32 = (u32_t)handle;
  RPC_NEXT32 = index;

  if ((r = rpc_query()) != OK)
	return r;

  attr_get(attr);

  return path_get(buf, size);
}

/*===========================================================================*
 *				hgfs_closedir				     *
 *===========================================================================*/
PUBLIC int hgfs_closedir(handle)
hgfs_dir_t handle;
{
/* Close an open directory.
 */

  RPC_REQUEST(HGFS_REQ_CLOSEDIR);
  RPC_NEXT32 = (u32_t)handle;

  return rpc_query();
}
