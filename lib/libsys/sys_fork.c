#include "syslib.h"

int sys_fork(parent, child, child_endpoint, flags, msgaddr)
endpoint_t parent;		/* process doing the fork */
endpoint_t child;		/* which proc has been created by the fork */
endpoint_t *child_endpoint;
u32_t flags;
vir_bytes *msgaddr;
{
/* A process has forked.  Tell the kernel. */

  message m;
  int r;

  m.m_lsys_krn_sys_fork.endpt = parent;
  m.m_lsys_krn_sys_fork.slot = child;
  m.m_lsys_krn_sys_fork.flags = flags;
  r = _kernel_call(SYS_FORK, &m);
  *child_endpoint = m.m_krn_lsys_sys_fork.endpt;
  *msgaddr = m.m_krn_lsys_sys_fork.msgaddr;
  return r;
}
