#include "syslib.h"

int sys_kill(proc_ep, signr)
endpoint_t proc_ep;		/* which proc_ep has exited */
int signr;			/* signal number: 1 - 16 */
{
/* A proc_ep has to be signaled via PM.  Tell the kernel. */
  message m;

  m.m_sigcalls.endpt = proc_ep;
  m.m_sigcalls.sig = signr;
  return(_kernel_call(SYS_KILL, &m));
}

