#include <sys/cdefs.h>
#include "namespace.h"
#include <lib.h>

#include <sys/stat.h>
#include <sys/time.h>
#include <fcntl.h>
#include <string.h>
#include <errno.h>

#ifdef __weak_alias
__weak_alias(lutimes, __lutimes50)
#endif

int lutimes(const char *name, const struct timeval tv[2])
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
  m.m_vfs_utimens.len = strlen(name) + 1;
  m.m_vfs_utimens.name = (char *) __UNCONST(name);
  if (tv == NULL) {
	m.m_vfs_utimens.atime = m.m_vfs_utimens.mtime = 0;
	m.m_vfs_utimens.ansec = m.m_vfs_utimens.mnsec = UTIME_NOW;
  }
  else {
	m.m_vfs_utimens.atime = tv[0].tv_sec;
	m.m_vfs_utimens.mtime = tv[1].tv_sec;
	m.m_vfs_utimens.ansec = tv[0].tv_usec * 1000;
	m.m_vfs_utimens.mnsec = tv[1].tv_usec * 1000;
  }
  m.m_vfs_utimens.flags = AT_SYMLINK_NOFOLLOW;

  return(_syscall(VFS_PROC_NR, VFS_UTIMENS, &m));
}
