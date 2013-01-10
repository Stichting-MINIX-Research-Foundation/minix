#include <sys/cdefs.h>
#include <lib.h>
#include "namespace.h"

#include <sys/wait.h>

#ifdef __weak_alias
__weak_alias(waitpid, _waitpid)
#endif

pid_t waitpid(pid_t pid, int *status, int options)
{
  message m;

  m.m1_i1 = pid;
  m.m1_i2 = options;
  if (_syscall(PM_PROC_NR, WAITPID, &m) < 0) return(-1);
  if (status != 0) *status = m.m2_i1;
  return m.m_type;
}
