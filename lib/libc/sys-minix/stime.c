#include <sys/cdefs.h>

#include "namespace.h"

#include <minix/sysutil.h>
#include <lib.h>
#include <string.h>
#include <time.h>

int stime(time_t *top)
{
  message m;

  memset(&m, 0, sizeof(m));
  m.PM_TIME_SEC = *top;
  return(_syscall(PM_PROC_NR, PM_STIME, &m));
}
