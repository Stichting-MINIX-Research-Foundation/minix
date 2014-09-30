#include <sys/cdefs.h>
#include "namespace.h"

#ifdef __weak_alias
__weak_alias(sprofile, _sprofile)
#endif

#include <lib.h>
#include <string.h>
#include <minix/profile.h>

int sprofile(int action,
		int size,
		int freq,
		int type,
		void *ctl_ptr,
		void *mem_ptr)
{
  message m;

  memset(&m, 0, sizeof(m));
  m.m_lc_pm_sprof.action	= action;
  m.m_lc_pm_sprof.mem_size	= size;
  m.m_lc_pm_sprof.freq		= freq;
  m.m_lc_pm_sprof.intr_type	= type;
  m.m_lc_pm_sprof.ctl_ptr	= (vir_bytes)ctl_ptr;
  m.m_lc_pm_sprof.mem_ptr	= (vir_bytes)mem_ptr;

  return _syscall(PM_PROC_NR, PM_SPROF, &m);
}

