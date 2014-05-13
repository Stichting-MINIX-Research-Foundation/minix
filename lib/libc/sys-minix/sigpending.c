#include <sys/cdefs.h>
#include <lib.h>
#include "namespace.h"

#include <string.h>
#include <signal.h>

int sigpending(set)
sigset_t *set;
{
  message m;

  memset(&m, 0, sizeof(m));
  if (_syscall(PM_PROC_NR, PM_SIGPENDING, &m) < 0) return(-1);
  *set = m.m_pm_lc_sigset.set;
  return(m.m_type);
}

#if defined(__minix) && defined(__weak_alias)
__weak_alias(sigpending, __sigpending14)
#endif
