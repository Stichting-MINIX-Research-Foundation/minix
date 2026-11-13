#include "syslib.h"

int sys_kill(endpoint_t proc_ep, int signr)
{
/* A proc_ep has to be signaled via PM.  Tell the kernel. */
  message m;

  m.m_sigcalls.endpt = proc_ep;
  m.m_sigcalls.sig = signr;
  return(_kernel_call(SYS_KILL, &m));
}

