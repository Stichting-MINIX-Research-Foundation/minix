#include <lib.h>
#define setgid	_setgid
#define setegid	_setegid
#include <unistd.h>

PUBLIC int setgid(gid_t grp)
{
  message m;

  m.m1_i1 = (int) grp;
  return(_syscall(PM_PROC_NR, SETGID, &m));
}

PUBLIC int setegid(gid_t grp)
{
  message m;

  m.m1_i1 = (int) grp;
  return(_syscall(PM_PROC_NR, SETEGID, &m));
}
