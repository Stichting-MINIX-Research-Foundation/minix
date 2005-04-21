#include "syslib.h"
#include <stdarg.h>
#include <unistd.h>

PUBLIC int sys_abort(int how, ...)
{
/* Something awful has happened.  Abandon ship. */

  message m;
  va_list ap;

  va_start(ap, how);
  if ((m.ABRT_HOW = how) == RBT_MONITOR) {
	m.ABRT_MON_PROC = va_arg(ap, int);
	m.ABRT_MON_ADDR = va_arg(ap, char *);
	m.ABRT_MON_LEN = va_arg(ap, size_t);
  }
  va_end(ap);

  return(_taskcall(SYSTASK, SYS_ABORT, &m));
}
