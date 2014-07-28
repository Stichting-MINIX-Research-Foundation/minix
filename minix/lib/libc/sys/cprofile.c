#include <sys/cdefs.h>
#include "namespace.h"

#ifdef __weak_alias
__weak_alias(cprofile, _cprofile)
#endif

#include <lib.h>
#include <string.h>
#include <minix/profile.h>

int cprofile(int action, int size, void *ctl_ptr, void *mem_ptr)
{
  message m;

  memset(&m, 0, sizeof(m));
  m.m_lc_pm_cprof.action	= action;
  m.m_lc_pm_cprof.mem_size	= size;
  m.m_lc_pm_cprof.ctl_ptr	= ctl_ptr;
  m.m_lc_pm_cprof.mem_ptr	= mem_ptr;

  return _syscall(PM_PROC_NR, PM_CPROF, &m);
}

