
#include "syslib.h"

#include <minix/safecopies.h>

PUBLIC int sys_vsafecopy(struct vscp_vec *vec, int els)
{
/* Vectored variant of sys_safecopy*. */

  message copy_mess;

  copy_mess.VSCP_VEC_ADDR = (char *) vec;
  copy_mess.VSCP_VEC_SIZE = els;

  return(_taskcall(SYSTASK, SYS_VSAFECOPY, &copy_mess));

}

