
#include "syslib.h"

#include <minix/safecopies.h>

int sys_safecopyfrom(endpoint_t src_e,
	cp_grant_id_t gr_id, vir_bytes offset,
	vir_bytes address, size_t bytes)
{
/* Transfer a block of data for which the other process has previously
 * given permission. 
 */

  message copy_mess;

  copy_mess.m_lsys_kern_safecopy.from_to = src_e;
  copy_mess.m_lsys_kern_safecopy.gid = gr_id;
  copy_mess.m_lsys_kern_safecopy.offset = offset;
  copy_mess.m_lsys_kern_safecopy.address = (void *)address;
  copy_mess.m_lsys_kern_safecopy.bytes = bytes;

  return(_kernel_call(SYS_SAFECOPYFROM, &copy_mess));

}

int sys_safecopyto(endpoint_t dst_e,
	cp_grant_id_t gr_id, vir_bytes offset,
	vir_bytes address, size_t bytes)
{
/* Transfer a block of data for which the other process has previously
 * given permission. 
 */

  message copy_mess;

  copy_mess.m_lsys_kern_safecopy.from_to = dst_e;
  copy_mess.m_lsys_kern_safecopy.gid = gr_id;
  copy_mess.m_lsys_kern_safecopy.offset = offset;
  copy_mess.m_lsys_kern_safecopy.address = (void *)address;
  copy_mess.m_lsys_kern_safecopy.bytes = bytes;

  return(_kernel_call(SYS_SAFECOPYTO, &copy_mess));

}
