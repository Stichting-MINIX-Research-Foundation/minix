#include "syslib.h"

int sys_vtimer(endpoint_t proc, int which, clock_t *newval, clock_t *oldval)
{
  message m;
  int r;

  m.VT_ENDPT = proc;
  m.VT_WHICH = which;
  if (newval != NULL) {
      m.VT_SET = 1;
      m.VT_VALUE = *newval;
  } else {
      m.VT_SET = 0;
  }

  r = _kernel_call(SYS_VTIMER, &m);

  if (oldval != NULL) {
      *oldval = m.VT_VALUE;
  }

  return(r);
}
