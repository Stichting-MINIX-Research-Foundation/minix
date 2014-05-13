#include <sys/cdefs.h>
#include "namespace.h"
#include <lib.h>

#include <unistd.h>
#include <string.h>

pid_t getsid(pid_t p)
{
  message m;

  memset(&m, 0, sizeof(m));
  m.m_lc_pm_getsid.pid = p;
  return(_syscall(PM_PROC_NR, PM_GETSID, &m));
}
