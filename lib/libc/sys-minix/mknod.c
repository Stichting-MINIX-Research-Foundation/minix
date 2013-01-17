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

  m.m1_i1 = strlen(name) + 1;
  m.m1_i2 = mode;
  m.m1_i3 = dev;
  m.m1_p1 = (char *) __UNCONST(name);
  m.m1_p2 = (char *) ((int) 0);		/* obsolete size field */
  return(_syscall(VFS_PROC_NR, MKNOD, &m));
}

#if defined(__minix) && defined(__weak_alias)
__weak_alias(mknod, __mknod50)
#endif
