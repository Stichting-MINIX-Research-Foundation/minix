#include <lib.h>

#define sprofile _sprofile

PUBLIC int sprofile(int action, int size, int freq, char *ctl_ptr, int *mem_ptr)
{
  message m;

  m.PROF_ACTION         = action;
  m.PROF_MEM_SIZE       = size;
  m.PROF_FREQ           = freq;
  m.PROF_CTL_PTR        = (void *) ctl_ptr;
  m.PROF_MEM_PTR        = (void *) mem_ptr;

  return _syscall(PM_PROC_NR, SPROF, &m);
}

