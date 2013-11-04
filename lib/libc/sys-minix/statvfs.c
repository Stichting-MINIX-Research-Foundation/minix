#include <sys/cdefs.h>
#include <lib.h>
#include "namespace.h"

#include <sys/statvfs.h>
#include <string.h>

#ifdef __weak_alias
__weak_alias(statvfs1, _statvfs1)
__weak_alias(statvfs, _statvfs)
#endif

int statvfs1(const char *name, struct statvfs *buffer, int flags)
{
  message m;

  memset(&m, 0, sizeof(m));
  m.VFS_STATVFS1_LEN = strlen(name) + 1;
  m.VFS_STATVFS1_NAME = (char *) __UNCONST(name);
  m.VFS_STATVFS1_BUF = (char *) buffer;
  m.VFS_STATVFS1_FLAGS = flags;
  return(_syscall(VFS_PROC_NR, VFS_STATVFS1, &m));
}

int statvfs(const char *name, struct statvfs *buffer)
{
  return statvfs1(name, buffer, ST_WAIT);
}
