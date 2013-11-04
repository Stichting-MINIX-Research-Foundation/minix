/* reboot.c

   author: Edvard Tuinder  v892231@si.hhs.NL
 */

#include <sys/cdefs.h>
#include <lib.h>
#include <unistd.h>
#include "namespace.h"

#include <string.h>
#include <sys/reboot.h>

int reboot(int how)
{
  message m;

  memset(&m, 0, sizeof(m));
  m.PM_REBOOT_HOW = how;
  return _syscall(PM_PROC_NR, PM_REBOOT, &m);
}
