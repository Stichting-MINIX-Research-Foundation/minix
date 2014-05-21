#include "syslib.h"

int sys_physcopy(src_proc, src_vir, dst_proc, dst_vir, bytes, flags)
endpoint_t src_proc;		/* source process */
vir_bytes src_vir;		/* source virtual address */
endpoint_t dst_proc;		/* destination process */
vir_bytes dst_vir;		/* destination virtual address */
phys_bytes bytes;		/* how many bytes */
int flags;			/* copy flags */
{
/* Transfer a block of data.  The source and destination can each either be a
 * process number or SELF (to indicate own process number). Virtual addresses 
 * are offsets within LOCAL_SEG (text, stack, data), or BIOS_SEG. 
 * Physicall addressing is also possible with PHYS_SEG.
 */

  message copy_mess;

  if (bytes == 0L) return(OK);
  copy_mess.m_lsys_krn_sys_copy.src_endpt = src_proc;
  copy_mess.m_lsys_krn_sys_copy.src_addr = src_vir;
  copy_mess.m_lsys_krn_sys_copy.dst_endpt = dst_proc;
  copy_mess.m_lsys_krn_sys_copy.dst_addr = dst_vir;
  copy_mess.m_lsys_krn_sys_copy.nr_bytes = bytes;
  copy_mess.m_lsys_krn_sys_copy.flags = flags;

  return(_kernel_call(SYS_PHYSCOPY, &copy_mess));
}
