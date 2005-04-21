#include <lib.h>
#define setuid	_setuid
#include <unistd.h>

PUBLIC int setuid(usr)
Uid_t usr;
{
  message m;

  m.m1_i1 = usr;
  return(_syscall(MM, SETUID, &m));
}
