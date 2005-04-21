#include "syslib.h"

PUBLIC int sys_kill(proc, signr)
int proc;			/* which proc has exited */
int signr;			/* signal number: 1 - 16 */
{
/* A proc has to be signaled via MM.  Tell the kernel. */
  message m;

  m.SIG_PROC = proc;
  m.SIG_NUMBER = signr;
  return(_taskcall(SYSTASK, SYS_KILL, &m));
}

