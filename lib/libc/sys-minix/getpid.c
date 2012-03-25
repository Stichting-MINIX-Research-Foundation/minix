#include <sys/cdefs.h>
#include "namespace.h"
#include <lib.h>

#include <unistd.h>

#ifdef __weak_alias
__weak_alias(getpid, _getpid)
#endif

pid_t getpid()
{
  message m;

  return(_syscall(PM_PROC_NR, MINIX_GETPID, &m));
}
