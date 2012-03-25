#include <sys/cdefs.h>
#include "namespace.h"
#include <lib.h>

#include <string.h>
#include <unistd.h>

#ifdef __weak_alias
__weak_alias(chown, _chown)
#endif

int chown(const char *name, uid_t owner, gid_t grp)
{
  message m;

  m.m1_i1 = strlen(name) + 1;
  m.m1_i2 = owner;
  m.m1_i3 = grp;
  m.m1_p1 = (char *) __UNCONST(name);
  return(_syscall(VFS_PROC_NR, CHOWN, &m));
}
