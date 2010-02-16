#include <lib.h>

#define cprofile _cprofile

PUBLIC int cprofile(int action, int size, char *ctl_ptr, int *mem_ptr)
{
  message m;

  m.PROF_ACTION         = action;
  m.PROF_MEM_SIZE       = size;
  m.PROF_CTL_PTR        = (void *) ctl_ptr;
  m.PROF_MEM_PTR        = (void *) mem_ptr;

  return _syscall(MM, CPROF, &m);
}

