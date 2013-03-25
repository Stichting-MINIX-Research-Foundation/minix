#include <sys/cdefs.h>
#include <lib.h>
#include "namespace.h"

#include <sys/statvfs.h>

#ifdef __weak_alias
__weak_alias(getvfsstat, _getvfsstat)
#endif

int getvfsstat(struct statvfs *buf, size_t bufsize, int flags)
{
  message m;

  m.VFS_GETVFSSTAT_BUF = (char *) buf;
  m.VFS_GETVFSSTAT_SIZE = bufsize;
  m.VFS_GETVFSSTAT_FLAGS = flags;
  return(_syscall(VFS_PROC_NR, GETVFSSTAT, &m));
}
