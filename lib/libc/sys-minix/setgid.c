#include <sys/cdefs.h>
#include <lib.h>
#include "namespace.h"

#ifdef __weak_alias
__weak_alias(setgid, _setgid)
__weak_alias(setegid, _setegid)
#endif

#include <unistd.h>

int setgid(gid_t grp)
{
  message m;

  m.m1_i1 = (int) grp;
  return(_syscall(PM_PROC_NR, SETGID, &m));
}

int setegid(gid_t grp)
{
  message m;

  m.m1_i1 = (int) grp;
  return(_syscall(PM_PROC_NR, SETEGID, &m));
}
