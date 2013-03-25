#include <sys/cdefs.h>
#include "namespace.h"
#include <lib.h>

#include <sys/stat.h>
#include <sys/time.h>
#include <string.h>
#include <errno.h>

#ifdef __weak_alias
__weak_alias(utimes, __utimes50)
#endif

int utimes(const char *name, const struct timeval tv[2])
{
  message m;

  if (name == NULL) {
	errno = EINVAL;
	return -1;
  }
  if (name[0] == '\0') { /* X/Open requirement */
	errno = ENOENT;
	return -1;
  }
  memset(&m, 0, sizeof(m));
  m.VFS_UTIMENS_LEN = strlen(name) + 1;
  m.VFS_UTIMENS_NAME = (char *) __UNCONST(name);
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
  m.VFS_UTIMENS_FLAGS = 0;

  return(_syscall(VFS_PROC_NR, VFS_UTIMENS, &m));
}
