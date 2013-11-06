#include <sys/cdefs.h>
#include "namespace.h"
#include <lib.h>

#include <string.h>
#include <unistd.h>

#ifdef __weak_alias
__weak_alias(close, _close)
#endif

int close(fd)
int fd;
{
  message m;

  memset(&m, 0, sizeof(m));
  m.VFS_CLOSE_FD = fd;
  return(_syscall(VFS_PROC_NR, VFS_CLOSE, &m));
}
