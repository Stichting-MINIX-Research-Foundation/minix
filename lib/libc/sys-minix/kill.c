#include <sys/cdefs.h>
#include "namespace.h"
#include <lib.h>

#include <string.h>
#include <signal.h>

#ifdef __weak_alias
__weak_alias(kill, _kill)
#endif

int kill(proc, sig)
pid_t proc;			/* which process is to be sent the signal */
int sig;			/* signal number */
{
  message m;

  memset(&m, 0, sizeof(m));
  m.PM_SIG_PID = proc;
  m.PM_SIG_NR = sig;
  return(_syscall(PM_PROC_NR, PM_KILL, &m));
}
