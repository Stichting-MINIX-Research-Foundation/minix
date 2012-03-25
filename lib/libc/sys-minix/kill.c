#include <sys/cdefs.h>
#include "namespace.h"
#include <lib.h>

#include <signal.h>

#ifdef __weak_alias
__weak_alias(kill, _kill)
#endif

int kill(proc, sig)
int proc;			/* which process is to be sent the signal */
int sig;			/* signal number */
{
  message m;

  m.m1_i1 = proc;
  m.m1_i2 = sig;
  return(_syscall(PM_PROC_NR, KILL, &m));
}
