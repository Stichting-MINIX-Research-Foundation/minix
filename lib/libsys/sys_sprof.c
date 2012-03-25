#include "syslib.h"

#if SPROFILE

/*===========================================================================*
 *                                sys_sprof				     *
 *===========================================================================*/
int sys_sprof(action, size, freq, type, endpt, ctl_ptr, mem_ptr)
int action; 				/* start/stop profiling */
int size;				/* available profiling memory */
int freq;				/* sample frequency */
int type;
endpoint_t endpt;			/* caller endpoint */
void *ctl_ptr;				/* location of info struct */
void *mem_ptr;				/* location of profiling memory */
{
  message m;

  m.PROF_ACTION         = action;
  m.PROF_MEM_SIZE       = size;
  m.PROF_FREQ           = freq;
  m.PROF_INTR_TYPE      = type;
  m.PROF_ENDPT		= endpt;
  m.PROF_CTL_PTR        = ctl_ptr;
  m.PROF_MEM_PTR        = mem_ptr;

  return(_kernel_call(SYS_SPROF, &m));
}

#endif

