#include <sys/cdefs.h>
#include "namespace.h"

#ifdef __weak_alias
#define sprofile _sprofile
__weak_alias(sprofile, _sprofile)
#endif

#include <lib.h>
#include <minix/profile.h>

int sprofile(int action,
		int size,
		int freq,
		int type,
		void *ctl_ptr,
		void *mem_ptr)
{
  message m;

  m.PROF_ACTION         = action;
  m.PROF_MEM_SIZE       = size;
  m.PROF_FREQ           = freq;
  m.PROF_INTR_TYPE      = type;
  m.PROF_CTL_PTR        = (void *) ctl_ptr;
  m.PROF_MEM_PTR        = (void *) mem_ptr;

  return _syscall(PM_PROC_NR, SPROF, &m);
}

