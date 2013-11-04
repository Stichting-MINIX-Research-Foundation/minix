#include <sys/cdefs.h>
#include "namespace.h"
#include <lib.h>

#include <string.h>
#include <unistd.h>

#ifdef __weak_alias
__weak_alias(getpgrp, _getpgrp)
#endif

pid_t getpgrp(void)
{
  message m;

  memset(&m, 0, sizeof(m));
  return(_syscall(PM_PROC_NR, PM_GETPGRP, &m));
}
