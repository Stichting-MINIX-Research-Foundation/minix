#include <sys/cdefs.h>
#include <lib.h>
#include "namespace.h"

#include <signal.h>

int sigpending(set)
sigset_t *set;
{
  message m;

  if (_syscall(PM_PROC_NR, SIGPENDING, &m) < 0) return(-1);
  memcpy(set, &m.SIG_MAP, sizeof(*set));
  return(m.m_type);
}

#if defined(__minix) && defined(__weak_alias)
__weak_alias(sigpending, __sigpending14)
#endif
