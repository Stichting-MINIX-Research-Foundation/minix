/* Part of libhgfs - (c) 2009, D.C. van Moolenbroek */

#include "inc.h"

#include <sys/stat.h>

/*===========================================================================*
 *				attr_get				     *
 *===========================================================================*/
PUBLIC void attr_get(attr)
struct hgfs_attr *attr;
{
/* Get attribute information from the RPC buffer, storing the requested parts
 * in the given attr structure.
 */
  mode_t mode;
  u32_t size_lo, size_hi;

  mode = (RPC_NEXT32) ? S_IFDIR : S_IFREG;

  size_lo = RPC_NEXT32;
  size_hi = RPC_NEXT32;
  if (attr->a_mask & HGFS_ATTR_SIZE)
	attr->a_size = make64(size_lo, size_hi);

  time_get((attr->a_mask & HGFS_ATTR_CRTIME) ? &attr->a_crtime : NULL);
  time_get((attr->a_mask & HGFS_ATTR_ATIME) ? &attr->a_atime : NULL);
  time_get((attr->a_mask & HGFS_ATTR_MTIME) ? &attr->a_mtime : NULL);
  time_get((attr->a_mask & HGFS_ATTR_CTIME) ? &attr->a_ctime : NULL);

  mode |= HGFS_PERM_TO_MODE(RPC_NEXT8);
  if (attr->a_mask & HGFS_ATTR_MODE) attr->a_mode = mode;
}

/*===========================================================================*
 *				hgfs_getattr				     *
 *===========================================================================*/
PUBLIC int hgfs_getattr(path, attr)
char *path;
struct hgfs_attr *attr;
{
/* Get selected attributes of a file by path name.
 */
  int r;

  RPC_REQUEST(HGFS_REQ_GETATTR);

  path_put(path);

  if ((r = rpc_query()) != OK)
	return r;

  attr_get(attr);

  return OK;
}

/*===========================================================================*
 *				hgfs_setattr				     *
 *===========================================================================*/
PUBLIC int hgfs_setattr(path, attr)
char *path;
struct hgfs_attr *attr;
{
/* Set selected attributes of a file by path name.
 */

  RPC_REQUEST(HGFS_REQ_SETATTR);

  RPC_NEXT8 = (attr->a_mask & HGFS_ATTR_ALL);

  RPC_NEXT32 = !!(S_ISDIR(attr->a_mode));
  RPC_NEXT32 = ex64lo(attr->a_size);
  RPC_NEXT32 = ex64hi(attr->a_size);

  time_put((attr->a_mask & HGFS_ATTR_CRTIME) ? &attr->a_crtime : NULL);
  time_put((attr->a_mask & HGFS_ATTR_ATIME) ? &attr->a_atime : NULL);
  time_put((attr->a_mask & HGFS_ATTR_MTIME) ? &attr->a_mtime : NULL);
  time_put((attr->a_mask & HGFS_ATTR_CTIME) ? &attr->a_ctime : NULL);

  RPC_NEXT8 = HGFS_MODE_TO_PERM(attr->a_mode);

  path_put(path);

  return rpc_query();
}
