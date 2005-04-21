#include <lib.h>
#define mount	_mount
#include <string.h>
#include <unistd.h>

PUBLIC int mount(special, name, rwflag)
char *name, *special;
int rwflag;
{
  message m;

  m.m1_i1 = strlen(special) + 1;
  m.m1_i2 = strlen(name) + 1;
  m.m1_i3 = rwflag;
  m.m1_p1 = special;
  m.m1_p2 = name;
  return(_syscall(FS, MOUNT, &m));
}
