/*
getgroups.c
*/

#include <sys/cdefs.h>
#include "namespace.h"
#include <lib.h>

#include <unistd.h>

#ifdef __weak_alias
__weak_alias(getgroups, _getgroups)
#endif


PUBLIC int getgroups(int ngroups, gid_t *arr)
{
  message m;
  m.m1_i1 = ngroups;
  m.m1_p1 = arr;

  return(_syscall(PM_PROC_NR, GETGROUPS, &m));
}

