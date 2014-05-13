/* reboot.c

   author: Edvard Tuinder  v892231@si.hhs.NL
 */

#include <sys/cdefs.h>
#include <lib.h>
#include <unistd.h>
#include "namespace.h"

#include <string.h>
#include <sys/reboot.h>

int reboot(int how, char *bootstr)
{
  message m;

  memset(&m, 0, sizeof(m));
  m.m_lc_pm_reboot.how = how;
  return _syscall(PM_PROC_NR, PM_REBOOT, &m);
}
