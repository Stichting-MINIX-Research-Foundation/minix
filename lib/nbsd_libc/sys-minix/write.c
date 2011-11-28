#include <sys/cdefs.h>
#include <lib.h>
#include "namespace.h"

#include <unistd.h>

#ifdef __weak_alias
__weak_alias(write, _write)
#endif

ssize_t write(int fd, const void *buffer, size_t nbytes)
{
  message m;

  m.m1_i1 = fd;
  m.m1_i2 = nbytes;
  m.m1_p1 = (char *) __UNCONST(buffer);
  return(_syscall(VFS_PROC_NR, WRITE, &m));
}

