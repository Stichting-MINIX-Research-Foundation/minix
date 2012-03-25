#include <sys/cdefs.h>
#include "namespace.h"
#include <lib.h>

#include <unistd.h>

#ifdef __weak_alias
__weak_alias(getgid, _getgid)
#endif

gid_t getgid()
{
  message m;

  return( (gid_t) _syscall(PM_PROC_NR, GETGID, &m));
}
