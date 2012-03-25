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

  va_start(argp, flags);
  if (flags & O_CREAT) {
	m.m1_i1 = strlen(name) + 1;
	m.m1_i2 = flags;
	/* Since it's a vararg parameter that is smaller than
	 * an int, the mode was passed as an int.
	 */
	m.m1_i3 = va_arg(argp, int);
	m.m1_p1 = (char *) __UNCONST(name);
  } else {
	_loadname(name, &m);
	m.m3_i2 = flags;
  }
  va_end(argp);
  return (_syscall(VFS_PROC_NR, OPEN, &m));
}
