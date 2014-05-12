#include <sys/cdefs.h>
#include <lib.h>
#include "namespace.h"

#include <string.h>
#include <sys/wait.h>

#ifdef __weak_alias
__weak_alias(wait, _wait)
#endif

pid_t wait(int * status)
{
  message m;

  memset(&m, 0, sizeof(m));
  m.m_lc_pm_waitpid.pid = -1;
  m.m_lc_pm_waitpid.options = 0;
  if (_syscall(PM_PROC_NR, PM_WAITPID, &m) < 0) return(-1);
  if (status != 0) *status = m.m_pm_lc_waitpid.status;
  return(m.m_type);
}
