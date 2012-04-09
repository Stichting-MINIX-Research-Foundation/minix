/* Part of libhgfs - (c) 2009, D.C. van Moolenbroek */

#include "inc.h"

/*===========================================================================*
 *				hgfs_queryvol				     *
 *===========================================================================*/
int hgfs_queryvol(path, free, total)
char *path;
u64_t *free;
u64_t *total;
{
/* Retrieve information about available and total volume space associated with
 * a given path.
 */
  u32_t lo, hi;
  int r;

  RPC_REQUEST(HGFS_REQ_QUERYVOL);

  path_put(path);

  /* It appears that this call always fails with EACCES ("permission denied")
   * on read-only folders. As far as I can tell, this is a VMware bug.
   */
  if ((r = rpc_query()) != OK)
	return r;

  lo = RPC_NEXT32;
  hi = RPC_NEXT32;
  *free = make64(lo, hi);

  lo = RPC_NEXT32;
  hi = RPC_NEXT32;
  *total = make64(lo, hi);

  return OK;
}
