#include <sys/cdefs.h>
#include <lib.h>
#include "namespace.h"

#include <signal.h>

int __sigreturn(void);

int sigaction(int sig, const struct sigaction *act, struct sigaction *oact)
{
  message m;

  m.m1_i2 = sig;

  m.m1_p1 = (char *) __UNCONST(act);
  m.m1_p2 = (char *) oact;
  m.m1_p3 = (char *) __sigreturn;

  return(_syscall(PM_PROC_NR, SIGACTION, &m));
}

#if defined(__minix) && defined(__weak_alias)
__weak_alias(sigaction, __sigaction14)
#endif
