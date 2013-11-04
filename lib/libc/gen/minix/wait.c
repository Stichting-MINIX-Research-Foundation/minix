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
  m.PM_WAITPID_PID = -1;
  m.PM_WAITPID_OPTIONS = 0;
  if (_syscall(PM_PROC_NR, PM_WAITPID, &m) < 0) return(-1);
  if (status != 0) *status = m.PM_WAITPID_STATUS;
  return(m.m_type);
}
