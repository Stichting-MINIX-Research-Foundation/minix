#include "fs.h"
#include "glo.h"


/*===========================================================================*
 *				fs_mount				     *
 *===========================================================================*/
int fs_mount(dev_t __unused dev, unsigned int __unused flags,
	struct fsdriver_node *node, unsigned int *res_flags)
{
/* Mount Pipe File Server. */

  /* This function does not do much. PFS has no root node, and VFS will ignore
   * the returned node details anyway. The whole idea is to provide symmetry
   * with other file systems, thus keeping libfsdriver simple and free of
   * special cases. Everything else (e.g., mounting PFS in VFS) is already an
   * exception anyway.
   */
  memset(node, 0, sizeof(*node));
  *res_flags = 0;

  return(OK);
}


/*===========================================================================*
 *				fs_unmount				     *
 *===========================================================================*/
void fs_unmount(void)
{
/* Unmount Pipe File Server. */

  if (busy)
	printf("PFS: unmounting while busy!\n"); /* nothing we can do anyway */
}
