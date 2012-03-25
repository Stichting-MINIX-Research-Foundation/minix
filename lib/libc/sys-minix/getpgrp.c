#include <sys/cdefs.h>
#include "namespace.h"
#include <lib.h>

#include <unistd.h>

#ifdef __weak_alias
__weak_alias(getpgrp, _getpgrp)
#endif

pid_t getpgrp()
{
  message m;

  return(_syscall(PM_PROC_NR, GETPGRP, &m));
}
