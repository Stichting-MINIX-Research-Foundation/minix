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
  m.PROF_ACTION         = action;
  m.PROF_MEM_SIZE       = size;
  m.PROF_CTL_PTR        = (void *) ctl_ptr;
  m.PROF_MEM_PTR        = (void *) mem_ptr;

  return _syscall(PM_PROC_NR, PM_CPROF, &m);
}

