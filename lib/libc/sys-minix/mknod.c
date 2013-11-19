#include <sys/cdefs.h>
#include "namespace.h"
#include <lib.h>

#include <string.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/stat.h>

int mknod(const char *name, mode_t mode, dev_t dev)
{
  message m;

  memset(&m, 0, sizeof(m));
  m.VFS_MKNOD_LEN = strlen(name) + 1;
  m.VFS_MKNOD_MODE = mode;
  m.VFS_MKNOD_DEV = dev;
  m.VFS_MKNOD_NAME = (char *) __UNCONST(name);
  return(_syscall(VFS_PROC_NR, VFS_MKNOD, &m));
}

#if defined(__minix) && defined(__weak_alias)
__weak_alias(mknod, __mknod50)
#endif
