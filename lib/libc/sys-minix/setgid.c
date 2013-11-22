#include <sys/cdefs.h>
#include <lib.h>
#include "namespace.h"

#include <string.h>

#ifdef __weak_alias
__weak_alias(setgid, _setgid)
__weak_alias(setegid, _setegid)
#endif

#include <unistd.h>

int setgid(gid_t grp)
{
  message m;

  memset(&m, 0, sizeof(m));
  m.PM_SETGID_GID = (int) grp;
  return(_syscall(PM_PROC_NR, PM_SETGID, &m));
}

int setegid(gid_t grp)
{
  message m;

  memset(&m, 0, sizeof(m));
  m.PM_SETGID_GID = (int) grp;
  return(_syscall(PM_PROC_NR, PM_SETEGID, &m));
}
