#include <sys/cdefs.h>
#include <lib.h>
#include "namespace.h"

#ifdef __weak_alias
__weak_alias(symlink, _symlink)
#endif

#include <string.h>
#include <unistd.h>

int symlink(name, name2)
const char *name, *name2;
{
  message m;

  m.m1_i1 = strlen(name) + 1;
  m.m1_i2 = strlen(name2) + 1;
  m.m1_p1 = (char *) name;
  m.m1_p2 = (char *) name2;
  return(_syscall(VFS_PROC_NR, SYMLINK, &m));
}
