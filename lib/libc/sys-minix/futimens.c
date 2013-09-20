#include <sys/cdefs.h>
#include "namespace.h"
#include <lib.h>

#include <string.h>
#include <sys/stat.h>

int futimens(int fd, const struct timespec tv[2])
{
  message m;
  static const struct timespec now[2] = { {0, UTIME_NOW}, {0, UTIME_NOW} };

  if (tv == NULL) tv = now;

  memset(&m, 0, sizeof(m));
  m.VFS_UTIMENS_FD = fd;
  /* For now just truncate to 32bit time_t values. */
  m.VFS_UTIMENS_ATIME = (int32_t)tv[0].tv_sec;
  m.VFS_UTIMENS_MTIME = (int32_t)tv[1].tv_sec;
  m.VFS_UTIMENS_ANSEC = (int32_t)tv[0].tv_nsec;
  m.VFS_UTIMENS_MNSEC = (int32_t)tv[1].tv_nsec;
  m.VFS_UTIMENS_NAME = NULL;
  m.VFS_UTIMENS_FLAGS = 0;

  return(_syscall(VFS_PROC_NR, VFS_UTIMENS, &m));
}
