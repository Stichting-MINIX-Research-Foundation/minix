#include <sys/cdefs.h>
#include "namespace.h"
#include <lib.h>

#include <sys/stat.h>
#include <string.h>

#ifdef __weak_alias
__weak_alias(mkdir, _mkdir)
#endif

int mkdir(const char *name, mode_t mode)
{
  message m;

  m.m1_i1 = strlen(name) + 1;
  m.m1_i2 = mode;
  m.m1_p1 = (char *) __UNCONST(name);
  return(_syscall(VFS_PROC_NR, MKDIR, &m));
}
