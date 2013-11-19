#include <sys/cdefs.h>
#include "namespace.h"
#include <lib.h>

#include <string.h>
#include <unistd.h>

#ifdef __weak_alias
__weak_alias(getpid, _getpid)
#endif

pid_t getpid(void)
{
  message m;

  memset(&m, 0, sizeof(m));
  return(_syscall(PM_PROC_NR, PM_GETPID, &m));
}
