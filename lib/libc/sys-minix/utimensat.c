#include <sys/cdefs.h>
#include "namespace.h"
#include <lib.h>

#include <sys/stat.h>
#include <fcntl.h>
#include <string.h>
#include <limits.h>
#include <errno.h>

/* Implement a very large but not complete subset of the utimensat()
 * Posix:2008/XOpen-7 function.
 * Are handled the following cases:
 * . utimensat(AT_FDCWD, "/some/absolute/path", , )
 * . utimensat(AT_FDCWD, "some/path", , )
 * . utimensat(fd, "/some/absolute/path", , ) although fd is useless here
 * Are not handled the following cases:
 * . utimensat(fd, "some/path", , ) path to a file relative to some open fd
 */
int utimensat(int fd, const char *name, const struct timespec tv[2],
    int flags)
{
  message m;
  static const struct timespec now[2] = { {0, UTIME_NOW}, {0, UTIME_NOW} };

  if (tv == NULL) tv = now;

  if (name == NULL) {
	errno = EINVAL;
	return -1;
  }
  if (name[0] == '\0') { /* POSIX requirement */
	errno = ENOENT;
	return -1;
  }
  if (fd != AT_FDCWD && name[0] != '/') { /* Not supported */
	errno = EINVAL;
	return -1;
  }

  if ((unsigned)flags > SHRT_MAX) {
	errno = EINVAL;
	return -1;
  }

  memset(&m, 0, sizeof(m));
  m.VFS_UTIMENS_LEN = strlen(name) + 1;
  m.VFS_UTIMENS_NAME = (char *) __UNCONST(name);
  /* For now just truncate time_t values to 32bits. */
  m.VFS_UTIMENS_ATIME = (int32_t)tv[0].tv_sec;
  m.VFS_UTIMENS_MTIME = (int32_t)tv[1].tv_sec;
  m.VFS_UTIMENS_ANSEC = (int32_t)tv[0].tv_nsec;
  m.VFS_UTIMENS_MNSEC = (int32_t)tv[1].tv_nsec;
  m.VFS_UTIMENS_FLAGS = flags;

  return(_syscall(VFS_PROC_NR, VFS_UTIMENS, &m));
}
