/*
setgroups.c
*/

#include <lib.h>
#define setgroups _setgroups
#include <unistd.h>

int setgroups(int ngroups, const gid_t *gidset)
{
  message m;

  m.m1_p1 = (char *) gidset;
  m.m1_i1 = ngroups;

  return(_syscall(PM_PROC_NR, SETGROUPS, &m));
}


