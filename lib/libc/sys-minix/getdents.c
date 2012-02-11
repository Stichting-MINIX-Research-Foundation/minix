#include <sys/cdefs.h>
#include "namespace.h"
#include <lib.h>

#include <dirent.h>

PUBLIC ssize_t getdents(fd, buffer, nbytes)
int fd;
char *buffer;
size_t nbytes;
{
  message m;

  m.m1_i1 = fd;
  m.m1_i2 = nbytes;
  m.m1_p1 = (char *) buffer;
  return _syscall(VFS_PROC_NR, GETDENTS, &m);
}
