/*
getgroups.c
*/

#include <sys/cdefs.h>
#include "namespace.h"
#include <lib.h>

#include <string.h>
#include <unistd.h>

int getgroups(int ngroups, gid_t *arr)
{
  message m;

  memset(&m, 0, sizeof(m));
  m.m_lc_pm_groups.num = ngroups;
  m.m_lc_pm_groups.ptr = (vir_bytes)arr;

  return(_syscall(PM_PROC_NR, PM_GETGROUPS, &m));
}

