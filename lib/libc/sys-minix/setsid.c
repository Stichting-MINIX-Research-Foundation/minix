#include <sys/cdefs.h>
#include <lib.h>
#include "namespace.h"

#include <string.h>
#include <unistd.h>

#ifdef __weak_alias
__weak_alias(setsid, _setsid)
#endif

pid_t setsid(void)
{
  message m;

  memset(&m, 0, sizeof(m));
  return(_syscall(PM_PROC_NR, PM_SETSID, &m));
}
