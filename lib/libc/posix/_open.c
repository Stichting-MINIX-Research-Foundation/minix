#include <lib.h>
#define open	_open
#include <fcntl.h>
#include <stdarg.h>
#include <string.h>

#if _ANSI
PUBLIC int open(const char *name, int flags, ...)
#else
PUBLIC int open(name, flags)
_CONST char *name;
int flags;
#endif
{
  va_list argp;
  message m;

  va_start(argp, flags);
  if (flags & O_CREAT) {
	m.m1_i1 = strlen(name) + 1;
	m.m1_i2 = flags;
	m.m1_i3 = va_arg(argp, _mnx_Mode_t);
	m.m1_p1 = (char *) name;
  } else {
	_loadname(name, &m);
	m.m3_i2 = flags;
  }
  va_end(argp);
  return (_syscall(FS, OPEN, &m));
}
