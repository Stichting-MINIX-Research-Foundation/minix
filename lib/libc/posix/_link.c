#include <lib.h>
#define link	_link
#include <string.h>
#include <unistd.h>

PUBLIC int link(name, name2)
_CONST char *name, *name2;
{
  message m;

  m.m1_i1 = strlen(name) + 1;
  m.m1_i2 = strlen(name2) + 1;
  m.m1_p1 = (char *) name;
  m.m1_p2 = (char *) name2;
  return(_syscall(FS, LINK, &m));
}
