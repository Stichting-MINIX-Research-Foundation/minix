#include <sys/cdefs.h>
#include <lib.h>
#include "namespace.h"

#include <string.h>
#include <unistd.h>

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
