#include "syslib.h"

PUBLIC int sys_physcopy(src_phys, dst_phys, bytes)
phys_bytes src_phys;		/* source physical address */
phys_bytes dst_phys;		/* destination physical address */
phys_bytes bytes;		/* how many bytes */
{
/* Transfer a block of data. This uses absolute memory addresses only. Use
 * sys_umap to convert a virtual address into a physical address if needed.
 */
  message copy_mess;

  if (bytes == 0L) return(OK);
  copy_mess.CP_SRC_ADDR = (long) src_phys;
  copy_mess.CP_DST_ADDR = (long) dst_phys;
  copy_mess.CP_NR_BYTES = (long) bytes;
  return(_taskcall(SYSTASK, SYS_PHYSCOPY, &copy_mess));
}
