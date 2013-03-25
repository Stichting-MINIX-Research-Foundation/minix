#include <sys/cdefs.h>
#include <lib.h>
#include "namespace.h"

#include <string.h>
#include <unistd.h>

#ifdef __weak_alias
__weak_alias(read, _read)
#endif

ssize_t read(int fd, void *buffer, size_t nbytes)
{
  message m;

  memset(&m, 0, sizeof(m));
  m.VFS_READWRITE_FD = fd;
  m.VFS_READWRITE_LEN = nbytes;
  m.VFS_READWRITE_BUF = (char *) buffer;
  return(_syscall(VFS_PROC_NR, VFS_READ, &m));
}
