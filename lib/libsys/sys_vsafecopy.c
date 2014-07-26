
#include "syslib.h"

#include <minix/safecopies.h>

int sys_vsafecopy(struct vscp_vec *vec, int els)
{
/* Vectored variant of sys_safecopy*. */

  message copy_mess;

  copy_mess.m_lsys_kern_vsafecopy.vec_addr = vec;
  copy_mess.m_lsys_kern_vsafecopy.vec_size = els;

  return(_kernel_call(SYS_VSAFECOPY, &copy_mess));

}

