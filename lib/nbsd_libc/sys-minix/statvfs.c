#include <sys/cdefs.h>
#include <lib.h>
#include "namespace.h"

#include <sys/statvfs.h>
#include <string.h>

#ifdef __weak_alias
__weak_alias(statvfs, _statvfs)
#endif

int statvfs(const char *name, struct statvfs *buffer)
{
  message m;

  m.STATVFS_LEN = strlen(name) + 1;
  m.STATVFS_NAME = (char *) __UNCONST(name);
  m.STATVFS_BUF = (char *) buffer;
  return(_syscall(VFS_PROC_NR, STATVFS, &m));
}
