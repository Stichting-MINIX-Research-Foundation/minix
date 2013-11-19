#include <sys/cdefs.h>
#include "namespace.h"
#include <lib.h>

#include <string.h>
#include <unistd.h>

#ifdef __weak_alias
__weak_alias(getgid, _getgid)
#endif

gid_t getgid(void)
{
  message m;

  memset(&m, 0, sizeof(m));
  return( (gid_t) _syscall(PM_PROC_NR, PM_GETGID, &m));
}
