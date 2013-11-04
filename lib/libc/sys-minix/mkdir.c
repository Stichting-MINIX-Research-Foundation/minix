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

#if 1 /* XXX OBSOLETE as of 3.3.0 */
  m.m1_i1 = strlen(name) + 1;
  m.m1_i2 = mode;
  m.m1_p1 = (char *) __UNCONST(name);
  return(_syscall(VFS_PROC_NR, 39, &m));
#else
  memset(&m, 0, sizeof(m));
  m.VFS_PATH_MODE = mode;
  _loadname(name, &m);
  return(_syscall(VFS_PROC_NR, VFS_MKDIR, &m));
#endif
}
