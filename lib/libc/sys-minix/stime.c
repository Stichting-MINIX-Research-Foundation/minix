#include <sys/cdefs.h>
#include <lib.h>
#include "namespace.h"

#include <string.h>
#include <time.h>

#ifdef __weak_alias
__weak_alias(stime, _stime)
#endif

int stime(time_t *top)
{
  message m;

  memset(&m, 0, sizeof(m));
  m.PM_TIME_SEC = *top;
  return(_syscall(PM_PROC_NR, PM_STIME, &m));
}
