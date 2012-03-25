
#include "syslib.h"

#include <minix/safecopies.h>

int sys_vsafecopy(struct vscp_vec *vec, int els)
{
/* Vectored variant of sys_safecopy*. */

  message copy_mess;

  copy_mess.VSCP_VEC_ADDR = (char *) vec;
  copy_mess.VSCP_VEC_SIZE = els;

  return(_kernel_call(SYS_VSAFECOPY, &copy_mess));

}

