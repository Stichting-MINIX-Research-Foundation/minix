#include "syslib.h"

PUBLIC int sys_oldsig(proc, sig, sighandler)
int proc;			/* process to be signaled  */
int sig;			/* signal number: 1 to _NSIG */
sighandler_t sighandler;	/* pointer to signal handler in user space */
{
/* A proc has to be signaled.  Tell the kernel. This function is obsolete. */

  message m;

  m.m6_i1 = proc;
  m.m6_i2 = sig;
  m.m6_f1 = sighandler;
  return(_taskcall(SYSTASK, SYS_OLDSIG, &m));
}
