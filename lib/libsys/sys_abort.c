#include "syslib.h"
#include <stdarg.h>
#include <unistd.h>

int sys_abort(int how)
{
/* Something awful has happened.  Abandon ship. */

  message m;

  m.ABRT_HOW = how;
  return(_kernel_call(SYS_ABORT, &m));
}
