#include <sys/cdefs.h>
#include <lib.h>
#include "namespace.h"

#include <string.h>
#include <sys/statvfs.h>

int getvfsstat(struct statvfs *buf, size_t bufsize, int flags)
{
  message m;

  memset(&m, 0, sizeof(m));
  m.VFS_GETVFSSTAT_BUF = (char *) buf;
  m.VFS_GETVFSSTAT_LEN = bufsize;
  m.VFS_GETVFSSTAT_FLAGS = flags;
  return(_syscall(VFS_PROC_NR, VFS_GETVFSSTAT, &m));
}
