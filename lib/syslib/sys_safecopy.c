
#include "syslib.h"

#include <minix/safecopies.h>

PUBLIC int sys_safecopyfrom(endpoint_t src_e,
	cp_grant_id_t gr_id, vir_bytes offset,
	vir_bytes address, size_t bytes,
	int my_seg)
{
/* Transfer a block of data for which the other process has previously
 * given permission. 
 */

  message copy_mess;

  copy_mess.SCP_FROM_TO = src_e;
  copy_mess.SCP_SEG = my_seg;
  copy_mess.SCP_GID = gr_id;
  copy_mess.SCP_OFFSET = (long) offset;
  copy_mess.SCP_ADDRESS = (char *) address;
  copy_mess.SCP_BYTES = (long) bytes;

  return(_taskcall(SYSTASK, SYS_SAFECOPYFROM, &copy_mess));

}

PUBLIC int sys_safecopyto(endpoint_t dst_e,
	cp_grant_id_t gr_id, vir_bytes offset,
	vir_bytes address, size_t bytes,
	int my_seg)
{
/* Transfer a block of data for which the other process has previously
 * given permission. 
 */

  message copy_mess;

  copy_mess.SCP_FROM_TO = dst_e;
  copy_mess.SCP_SEG = my_seg;
  copy_mess.SCP_GID = gr_id;
  copy_mess.SCP_OFFSET = (long) offset;
  copy_mess.SCP_ADDRESS = (char *) address;
  copy_mess.SCP_BYTES = (long) bytes;

  return(_taskcall(SYSTASK, SYS_SAFECOPYTO, &copy_mess));

}
