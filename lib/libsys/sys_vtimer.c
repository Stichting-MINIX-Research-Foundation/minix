#include "syslib.h"

int sys_vtimer(proc, which, newval, oldval)
endpoint_t proc;		/* proc to retrieve/set the timer for */
int which;			/* timer to retrieve/set */
clock_t *newval;		/* if non-NULL, set to this new value */
clock_t *oldval;		/* if non-NULL, old value is stored here */
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
