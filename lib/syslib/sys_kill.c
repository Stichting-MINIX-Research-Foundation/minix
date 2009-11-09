#include "syslib.h"

PUBLIC int sys_kill(proc_ep, signr)
endpoint_t proc_ep;		/* which proc_ep has exited */
int signr;			/* signal number: 1 - 16 */
{
/* A proc_ep has to be signaled via MM.  Tell the kernel. */
  message m;

  m.SIG_ENDPT = proc_ep;
  m.SIG_NUMBER = signr;
  return(_taskcall(SYSTASK, SYS_KILL, &m));
}

