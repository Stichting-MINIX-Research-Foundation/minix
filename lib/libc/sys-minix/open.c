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
	m.m_lc_vfs_creat.len = strlen(name) + 1;
	m.m_lc_vfs_creat.flags = flags;
	m.m_lc_vfs_creat.mode = va_arg(argp, mode_t);
	m.m_lc_vfs_creat.name = (vir_bytes)name;
	call = VFS_CREAT;
  } else {
	_loadname(name, &m);
	m.m_lc_vfs_path.flags = flags;
	call = VFS_OPEN;
  }
  va_end(argp);
  return (_syscall(VFS_PROC_NR, call, &m));
}
