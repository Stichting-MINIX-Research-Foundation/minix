#include <sys/cdefs.h>
#include <lib.h>
#include "namespace.h"

#include <string.h>

#include <unistd.h>

int setgid(gid_t grp)
{
  message m;

  memset(&m, 0, sizeof(m));
  m.m_lc_pm_setgid.gid = grp;
  return(_syscall(PM_PROC_NR, PM_SETGID, &m));
}

int setegid(gid_t grp)
{
  message m;

  memset(&m, 0, sizeof(m));
  m.m_lc_pm_setgid.gid = grp;
  return(_syscall(PM_PROC_NR, PM_SETEGID, &m));
}
