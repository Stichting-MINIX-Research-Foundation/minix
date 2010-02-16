#include <lib.h>
#define getsigset	_getsigset
#include <unistd.h>


PUBLIC int getsigset(sp)
sigset_t *sp;				/* where to put it */
{
  message m;
  m.m2_i1 = SELF;			/* request own signal set */
  if (_syscall(PM_PROC_NR, PROCSTAT, &m) < 0) return(-1);
  *sp = m.m2_l1;
  return(0);
}

