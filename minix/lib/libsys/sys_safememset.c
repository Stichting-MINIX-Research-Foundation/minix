#include "syslib.h"

#include <minix/safecopies.h>

int sys_safememset(endpoint_t dst_e, cp_grant_id_t gr_id,
	vir_bytes offset, int pattern, size_t len)
{
/* memset() a block of data using pattern */

  message copy_mess;

  copy_mess.SMS_DST = dst_e;
  copy_mess.SMS_GID = gr_id;
  copy_mess.SMS_OFFSET = (long) offset;
  copy_mess.SMS_PATTERN = pattern;
  copy_mess.SMS_BYTES = (long) len;

  return(_kernel_call(SYS_SAFEMEMSET, &copy_mess));
}
