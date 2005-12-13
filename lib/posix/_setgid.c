#include <lib.h>
#define setgid	_setgid
#define setegid	_setegid
#include <unistd.h>

PUBLIC int setgid(grp)
gid_t grp;
{
  message m;

  m.m1_i1 = (int) grp;
  return(_syscall(MM, SETGID, &m));
}

PUBLIC int setegid(grp)
gid_t grp;
{
  message m;

  m.m1_i1 = (int) grp;
  return(_syscall(MM, SETEGID, &m));
}
