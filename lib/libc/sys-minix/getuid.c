#include <sys/cdefs.h>
#include "namespace.h"
#include <lib.h>

#include <unistd.h>

#ifdef __weak_alias
__weak_alias(getuid, _getuid)
#endif

uid_t getuid()
{
  message m;

  return( (uid_t) _syscall(PM_PROC_NR, GETUID, &m));
}
