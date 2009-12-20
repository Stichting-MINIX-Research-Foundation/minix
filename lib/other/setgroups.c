/*
setgroups.c
*/

#include <lib.h>
#include <unistd.h>

int setgroups(int ngroups, const gid_t *gidset)
{
  message m;

  m.m1_p1 = gidset;
  m.m1_i1 = ngroups;

  return(_syscall(MM, SETGROUPS, &m));
}


