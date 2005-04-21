/* reboot.c - Systemcall interface to mm/signal.c::do_reboot()

   author: Edvard Tuinder  v892231@si.hhs.NL
 */

#include <lib.h>
#define reboot	_reboot
#include <unistd.h>
#include <stdarg.h>

int reboot(int how, ...)
{
  message m;
  va_list ap;

  va_start(ap, how);
  if ((m.m1_i1 = how) == RBT_MONITOR) {
	m.m1_p1 = va_arg(ap, char *);
	m.m1_i2 = va_arg(ap, size_t);
  }
  va_end(ap);

  return _syscall(MM, REBOOT, &m);
}
