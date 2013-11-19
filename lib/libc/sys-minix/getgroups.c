/*
getgroups.c
*/

#include <sys/cdefs.h>
#include "namespace.h"
#include <lib.h>

#include <string.h>
#include <unistd.h>

#ifdef __weak_alias
__weak_alias(getgroups, _getgroups)
#endif


int getgroups(int ngroups, gid_t *arr)
{
  message m;

  memset(&m, 0, sizeof(m));
  m.PM_GROUPS_NUM = ngroups;
  m.PM_GROUPS_PTR = (char *) arr;

  return(_syscall(PM_PROC_NR, PM_GETGROUPS, &m));
}

