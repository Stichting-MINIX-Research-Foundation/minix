#include <sys/cdefs.h>
#include <lib.h>
#include "namespace.h"

#include <unistd.h>

#ifdef __weak_alias
__weak_alias(setuid, _setuid)
__weak_alias(seteuid, _seteuid)
#endif

int setuid(uid_t usr)
{
  message m;

  m.m1_i1 = usr;
  return(_syscall(PM_PROC_NR, SETUID, &m));
}

int seteuid(uid_t usr)
{
  message m;

  m.m1_i1 = usr;
  return(_syscall(PM_PROC_NR, SETEUID, &m));
}
