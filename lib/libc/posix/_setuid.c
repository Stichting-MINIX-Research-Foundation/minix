#include <lib.h>
#define setuid	_setuid
#define seteuid	_seteuid
#include <unistd.h>

PUBLIC int setuid(uid_t usr)
{
  message m;

  m.m1_i1 = usr;
  return(_syscall(MM, SETUID, &m));
}

PUBLIC int seteuid(uid_t usr)
{
  message m;

  m.m1_i1 = usr;
  return(_syscall(MM, SETEUID, &m));
}
