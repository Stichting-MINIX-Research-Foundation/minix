#include "syslib.h"
#include <stdarg.h>
#include <unistd.h>

int sys_abort(int how)
{
/* Something awful has happened.  Abandon ship. */

  message m;

  m.m_lsys_krn_sys_abort.how = how;
  return(_kernel_call(SYS_ABORT, &m));
}
