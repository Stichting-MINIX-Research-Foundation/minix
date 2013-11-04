#include <sys/cdefs.h>
#include <lib.h>
#include "namespace.h"

#include <string.h>
#include <signal.h>

int __sigreturn(void);

int sigaction(int sig, const struct sigaction *act, struct sigaction *oact)
{
  message m;

  memset(&m, 0, sizeof(m));
  m.PM_SIG_NR = sig;
  m.PM_SIG_ACT = (char *) __UNCONST(act);
  m.PM_SIG_OACT = (char *) oact;
  m.PM_SIG_RET = (char *) __sigreturn;

  return(_syscall(PM_PROC_NR, PM_SIGACTION, &m));
}

#if defined(__minix) && defined(__weak_alias)
__weak_alias(sigaction, __sigaction14)
#endif
