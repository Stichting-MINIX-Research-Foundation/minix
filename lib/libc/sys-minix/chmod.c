#include <sys/cdefs.h>
#include "namespace.h"
#include <lib.h>

#include <sys/stat.h>

#ifdef __weak_alias
__weak_alias(chmod, _chmod)
#endif

int chmod(const char *name, mode_t mode)
{
  message m;

  m.m3_i2 = mode;
  _loadname(name, &m);
  return(_syscall(VFS_PROC_NR, CHMOD, &m));
}
