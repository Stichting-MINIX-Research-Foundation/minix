#include <sys/cdefs.h>
#include "namespace.h"
#include <lib.h>

#include <fcntl.h>
#include <stdarg.h>
#include <string.h>

#ifdef __weak_alias
__weak_alias(open, _open)
#endif

int open(const char *name, int flags, ...)
{
  va_list argp;
  message m;
  int call;

  memset(&m, 0, sizeof(m));
  va_start(argp, flags);
  if (flags & O_CREAT) {
	m.VFS_CREAT_LEN = strlen(name) + 1;
	m.VFS_CREAT_FLAGS = flags;
	/* Since it's a vararg parameter that is smaller than
	 * an int, the mode was passed as an int.
	 */
	m.VFS_CREAT_MODE = va_arg(argp, int);
	m.VFS_CREAT_NAME = (char *) __UNCONST(name);
#if 1 /* XXX OBSOLETE as of 3.3.0 */
	call = VFS_OPEN;
#else
	call = VFS_CREAT;
#endif
  } else {
	_loadname(name, &m);
	m.VFS_PATH_FLAGS = flags;
	call = VFS_OPEN;
  }
  va_end(argp);
  return (_syscall(VFS_PROC_NR, call, &m));
}
