#include "syslib.h"

PUBLIC int sys_vircopy(src_proc, src_seg, src_vir, 
	dst_proc, dst_seg, dst_vir, bytes)
int src_proc;			/* source process */
int src_seg;			/* source memory segment */
vir_bytes src_vir;		/* source virtual address */
int dst_proc;			/* destination process */
int dst_seg;			/* destination memory segment */
vir_bytes dst_vir;		/* destination virtual address */
phys_bytes bytes;		/* how many bytes */
{
/* Transfer a block of data.  The source and destination can each either be a
 * process number or SELF (to indicate own process number). Virtual addresses 
 * are offsets within LOCAL_SEG (text, stack, data), REMOTE_SEG, or BIOS_SEG. 
 */

  message copy_mess;

  if (bytes == 0L) return(OK);
  copy_mess.CP_SRC_PROC_NR = src_proc;
  copy_mess.CP_SRC_SPACE = src_seg;
  copy_mess.CP_SRC_ADDR = (long) src_vir;
  copy_mess.CP_DST_PROC_NR = dst_proc;
  copy_mess.CP_DST_SPACE = dst_seg;
  copy_mess.CP_DST_ADDR = (long) dst_vir;
  copy_mess.CP_NR_BYTES = (long) bytes;
  return(_taskcall(SYSTASK, SYS_VIRCOPY, &copy_mess));
}
