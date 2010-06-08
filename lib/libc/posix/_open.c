#include <lib.h>
#define open	_open
#include <fcntl.h>
#include <stdarg.h>
#include <string.h>

#if _ANSI
PUBLIC int open(const char *name, int flags, ...)
#else
PUBLIC int open(const char *name, int flags)
#endif
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
	m.m1_p1 = (char *) name;
  } else {
	_loadname(name, &m);
	m.m3_i2 = flags;
  }
  va_end(argp);
  return (_syscall(VFS_PROC_NR, OPEN, &m));
}
