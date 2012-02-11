#include <sys/cdefs.h>
#include <lib.h>
#include "namespace.h"

#include <signal.h>

int sigpending(set)
sigset_t *set;
{
  message m;

  if (_syscall(PM_PROC_NR, SIGPENDING, &m) < 0) return(-1);
  *set = (sigset_t) m.m2_l1;
  return(m.m_type);
}
