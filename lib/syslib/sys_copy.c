#include "syslib.h"

PUBLIC int sys_copy(src_proc, src_seg, src_vir,
					dst_proc, dst_seg, dst_vir, bytes)
int src_proc;			/* source process */
int src_seg;			/* source segment: T, D, or S */
phys_bytes src_vir;		/* source virtual address (phys addr for ABS)*/
int dst_proc;			/* dest process */
int dst_seg;			/* dest segment: T, D, or S */
phys_bytes dst_vir;		/* dest virtual address (phys addr for ABS) */
phys_bytes bytes;		/* how many bytes */
{
/* Transfer a block of data.  The source and destination can each either be a
 * process number, SELF (to set own process number), or ABS (to indicate the 
 * use of an absolute memory address).
 */

  message copy_mess;

  if (bytes == 0L) return(OK);
  copy_mess.CP_SRC_SPACE = src_seg;
  copy_mess.CP_SRC_PROC_NR = src_proc;
  copy_mess.CP_SRC_ADDR = (long) src_vir;

  copy_mess.CP_DST_SPACE = dst_seg;
  copy_mess.CP_DST_PROC_NR = dst_proc;
  copy_mess.CP_DST_ADDR = (long) dst_vir;

  copy_mess.CP_NR_BYTES = (long) bytes;
  return(_taskcall(SYSTASK, SYS_COPY, &copy_mess));
}
