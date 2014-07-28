#include <sys/cdefs.h>
#include <lib.h>
#include "namespace.h"

#include <string.h>
#include <unistd.h>

int setgroups(int ngroups, const gid_t *gidset)
{
  message m;

  memset(&m, 0, sizeof(m));
  m.m_lc_pm_groups.ptr = (vir_bytes)gidset;
  m.m_lc_pm_groups.num = ngroups;

  return(_syscall(PM_PROC_NR, PM_SETGROUPS, &m));
}
