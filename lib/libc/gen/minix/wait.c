#include <sys/cdefs.h>
#include <lib.h>
#include "namespace.h"

#include <sys/wait.h>

#ifdef __weak_alias
__weak_alias(wait, _wait)
#endif

pid_t wait(int * status)
{
  message m;

  if (_syscall(PM_PROC_NR, WAIT, &m) < 0) return(-1);
  if (status != 0) *status = m.m2_i1;
  return(m.m_type);
}
