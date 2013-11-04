#include <sys/cdefs.h>
#include <lib.h>
#include "namespace.h"

#include <string.h>
#include <unistd.h>

#ifdef __weak_alias
__weak_alias(setuid, _setuid)
__weak_alias(seteuid, _seteuid)
#endif

int setuid(uid_t usr)
{
  message m;

  memset(&m, 0, sizeof(m));
  m.PM_SETUID_UID = usr;
  return(_syscall(PM_PROC_NR, PM_SETUID, &m));
}

int seteuid(uid_t usr)
{
  message m;

  memset(&m, 0, sizeof(m));
  m.PM_SETUID_UID = usr;
  return(_syscall(PM_PROC_NR, PM_SETEUID, &m));
}
