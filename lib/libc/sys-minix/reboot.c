/* reboot.c - Systemcall interface to mm/signal.c::do_reboot()

   author: Edvard Tuinder  v892231@si.hhs.NL
 */

#include <sys/cdefs.h>
#include <lib.h>
#include "namespace.h"

#include <unistd.h>
#include <sys/reboot.h>
#include <stdarg.h>

int reboot(int how)
{
  message m;

  m.m1_i1 = how;
  return _syscall(PM_PROC_NR, REBOOT, &m);
}
