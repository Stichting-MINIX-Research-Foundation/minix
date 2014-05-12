#include <sys/cdefs.h>
#include "namespace.h"
#include <lib.h>

#include <fcntl.h>
#include <stdarg.h>
#include <string.h>

int open(const char *name, int flags, ...)
{
  va_list argp;
  message m;
  int call;

  memset(&m, 0, sizeof(m));
  va_start(argp, flags);
  /* Depending on whether O_CREAT is set, a different message layout is used,
   * and therefore a different call number as well.
   */
  if (flags & O_CREAT) {
	m.VFS_CREAT_LEN = strlen(name) + 1;
	m.VFS_CREAT_FLAGS = flags;
	m.VFS_CREAT_MODE = va_arg(argp, int);
	m.VFS_CREAT_NAME = (char *) __UNCONST(name);
	call = VFS_CREAT;
  } else {
	_loadname(name, &m);
	m.m_lc_vfs_path.flags = flags;
	call = VFS_OPEN;
  }
  va_end(argp);
  return (_syscall(VFS_PROC_NR, call, &m));
}
