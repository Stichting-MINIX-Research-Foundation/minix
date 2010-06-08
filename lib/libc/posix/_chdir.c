#include <lib.h>
#define chdir	_chdir
#define fchdir	_fchdir
#include <unistd.h>

PUBLIC int chdir(name)
_CONST char *name;
{
  message m;

  _loadname(name, &m);
  return(_syscall(VFS_PROC_NR, CHDIR, &m));
}

PUBLIC int fchdir(fd)
int fd;
{
  message m;

  m.m1_i1 = fd;
  return(_syscall(VFS_PROC_NR, FCHDIR, &m));
}
