#include <sys/cdefs.h>
#include <lib.h>
#include "namespace.h"

#include <unistd.h>

#ifdef __weak_alias
__weak_alias(setsid, _setsid)
#endif

pid_t setsid()
{
  message m;

  return(_syscall(PM_PROC_NR, SETSID, &m));
}
