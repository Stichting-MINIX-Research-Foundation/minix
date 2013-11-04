#include <sys/cdefs.h>
#include "namespace.h"
#include <lib.h>

#include <string.h>
#include <sys/stat.h>
#include <sys/time.h>

#ifdef __weak_alias
__weak_alias(futimes, __futimes50)
#endif

int futimes(int fd, const struct timeval tv[2])
{
  message m;

  memset(&m, 0, sizeof(m));
  m.VFS_UTIMENS_FD = fd;
  if (tv == NULL) {
	m.VFS_UTIMENS_ATIME = m.VFS_UTIMENS_MTIME = 0;
	m.VFS_UTIMENS_ANSEC = m.VFS_UTIMENS_MNSEC = UTIME_NOW;
  }
  else {
	m.VFS_UTIMENS_ATIME = tv[0].tv_sec;
	m.VFS_UTIMENS_MTIME = tv[1].tv_sec;
	m.VFS_UTIMENS_ANSEC = tv[0].tv_usec * 1000;
	m.VFS_UTIMENS_MNSEC = tv[1].tv_usec * 1000;
  }
  m.VFS_UTIMENS_NAME = NULL;
  m.VFS_UTIMENS_FLAGS = 0;

  return(_syscall(VFS_PROC_NR, VFS_UTIMENS, &m));
}
