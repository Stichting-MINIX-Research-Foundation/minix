#include <lib.h>
#define chdir	_chdir
#include <unistd.h>

PUBLIC int chdir(name)
_CONST char *name;
{
  message m;

  _loadname(name, &m);
  return(_syscall(FS, CHDIR, &m));
}

#if 0
PUBLIC int fchdir(fd)
int fd;
{
  message m;

  m.m1_i1 = fd;
  return(_syscall(FS, FCHDIR, &m));
}
#endif

