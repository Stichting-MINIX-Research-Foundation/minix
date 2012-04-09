/* Part of libhgfs - (c) 2009, D.C. van Moolenbroek */

#include "inc.h"

/*===========================================================================*
 *				hgfs_init				     *
 *===========================================================================*/
int hgfs_init()
{
/* Initialize the library. Return OK on success, or a negative error code
 * otherwise. If EAGAIN is returned, shared folders are disabled.
 */

  time_init();

  return rpc_open();
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
