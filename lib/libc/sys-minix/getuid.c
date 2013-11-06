#include <sys/cdefs.h>
#include "namespace.h"
#include <lib.h>

#include <string.h>
#include <unistd.h>

#ifdef __weak_alias
__weak_alias(getuid, _getuid)
#endif

uid_t getuid(void)
{
  message m;

  memset(&m, 0, sizeof(m));
  return( (uid_t) _syscall(PM_PROC_NR, PM_GETUID, &m));
}
