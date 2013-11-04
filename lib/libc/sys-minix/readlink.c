#include <sys/cdefs.h>
#include <lib.h>
#include "namespace.h"

#include <unistd.h>
#include <string.h>

#ifdef __weak_alias
__weak_alias(readlink, _readlink)
#endif

ssize_t readlink(const char *name, char *buffer, size_t bufsiz)
{
  message m;

  memset(&m, 0, sizeof(m));
  m.VFS_READLINK_NAMELEN = strlen(name) + 1;
  m.VFS_READLINK_BUFSIZE = bufsiz;
  m.VFS_READLINK_NAME = (char *) __UNCONST(name);
  m.VFS_READLINK_BUF = (char *) buffer;

  return(_syscall(VFS_PROC_NR, VFS_READLINK, &m));
}
