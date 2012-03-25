#include <sys/cdefs.h>
#include "namespace.h"
#include <lib.h>

#include <unistd.h>

#ifdef __weak_alias
__weak_alias(chdir, _chdir)
__weak_alias(fchdir, _fchdir)
#endif

int chdir(name)
const char *name;
{
  message m;

  _loadname(name, &m);
  return(_syscall(VFS_PROC_NR, CHDIR, &m));
}

int fchdir(fd)
int fd;
{
  message m;

  m.m1_i1 = fd;
  return(_syscall(VFS_PROC_NR, FCHDIR, &m));
}
