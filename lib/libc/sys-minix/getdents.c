#include <sys/cdefs.h>
#include "namespace.h"
#include <lib.h>

#include <dirent.h>

ssize_t getdents(int fd, char *buffer, size_t nbytes)
{
  message m;

  m.m1_i1 = fd;
  m.m1_i2 = nbytes;
  m.m1_p1 = (char *) buffer;
  return _syscall(VFS_PROC_NR, GETDENTS, &m);
}

#if defined(__minix) && defined(__weak_alias)
__weak_alias(getdents, __getdents30)
#endif
