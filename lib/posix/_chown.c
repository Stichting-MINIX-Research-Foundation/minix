#include <lib.h>
#define chown	_chown
#include <string.h>
#include <unistd.h>

PUBLIC int chown(name, owner, grp)
_CONST char *name;
Uid_t owner;
Gid_t grp;
{
  message m;

  m.m1_i1 = strlen(name) + 1;
  m.m1_i2 = owner;
  m.m1_i3 = grp;
  m.m1_p1 = (char *) name;
  return(_syscall(FS, CHOWN, &m));
}
