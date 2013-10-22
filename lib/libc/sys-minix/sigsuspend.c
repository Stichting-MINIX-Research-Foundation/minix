#include <sys/cdefs.h>
#include <lib.h>
#include "namespace.h"

#include <signal.h>

int sigsuspend(set)
const sigset_t *set;
{
  message m;
  memset(&m, 0, sizeof(m));
  memcpy(&m.SIG_MAP, set, sizeof(*set));
  return(_syscall(PM_PROC_NR, SIGSUSPEND, &m));
}

#if defined(__minix) && defined(__weak_alias)
__weak_alias(sigsuspend, __sigsuspend14)
#endif
