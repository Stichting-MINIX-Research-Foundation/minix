/*
getgroups.c
*/

#include <lib.h>
#define getgroups _getgroups
#include <unistd.h>

PUBLIC int getgroups(int ngroups, gid_t *arr)
{
  message m;
  m.m1_i1 = ngroups;
  m.m1_p1 = arr;

  return(_syscall(MM, GETGROUPS, &m));
}

