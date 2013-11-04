#include <sys/cdefs.h>
#include "namespace.h"
#include <lib.h>

#include <string.h>
#include <dirent.h>

ssize_t getdents(int fd, char *buffer, size_t nbytes)
{
  message m;

  memset(&m, 0, sizeof(m));
  m.VFS_READWRITE_FD = fd;
  m.VFS_READWRITE_LEN = nbytes;
  m.VFS_READWRITE_BUF = (char *) buffer;
  return _syscall(VFS_PROC_NR, VFS_GETDENTS, &m);
}

#if defined(__minix) && defined(__weak_alias)
__weak_alias(getdents, __getdents30)
#endif
