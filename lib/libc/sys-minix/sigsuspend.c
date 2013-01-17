#include <sys/cdefs.h>
#include <lib.h>
#include "namespace.h"

#include <signal.h>

int sigsuspend(set)
const sigset_t *set;
{
  message m;

  m.m2_l1 = (long) *set;
  return(_syscall(PM_PROC_NR, SIGSUSPEND, &m));
}

#if defined(__minix) && defined(__weak_alias)
__weak_alias(sigsuspend, __sigsuspend14)
#endif
