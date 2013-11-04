#include <sys/cdefs.h>
#include "namespace.h"
#include <lib.h>

#include <string.h>
#include <sys/stat.h>

#ifdef __weak_alias
__weak_alias(chmod, _chmod)
#endif

int chmod(const char *name, mode_t mode)
{
  message m;

  memset(&m, 0, sizeof(m));
  m.VFS_PATH_MODE = mode;
  _loadname(name, &m);
  return(_syscall(VFS_PROC_NR, VFS_CHMOD, &m));
}
