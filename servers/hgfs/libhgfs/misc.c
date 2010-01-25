/* Part of libhgfs - (c) 2009, D.C. van Moolenbroek */

#include "inc.h"

/*===========================================================================*
 *				hgfs_init				     *
 *===========================================================================*/
PUBLIC int hgfs_init()
{
/* Initialize the library. Return OK on success, or a negative error code
 * otherwise. If EAGAIN is returned, shared folders are disabled; in that
 * case, other operations may be tried (and possibly succeed).
 */

  time_init();

  return rpc_open();
}

/*===========================================================================*
 *				hgfs_cleanup				     *
 *===========================================================================*/
PUBLIC void hgfs_cleanup()
{
/* Clean up state.
 */

  rpc_close();
}

/*===========================================================================*
 *				hgfs_enabled				     *
 *===========================================================================*/
PUBLIC int hgfs_enabled()
{
/* Check if shared folders are enabled. Return OK if so, EAGAIN if not, and
 * another negative error code on error.
 */

  return rpc_test();
}

/*===========================================================================*
 *				hgfs_queryvol				     *
 *===========================================================================*/
PUBLIC int hgfs_queryvol(path, free, total)
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
