#include <sys/cdefs.h>
#include <lib.h>
#include "namespace.h"

#include <string.h>
#include <stdio.h>

#ifdef __weak_alias
__weak_alias(__posix_rename, rename)
#endif

int rename(const char *name, const char *name2)
{
  message m;

  memset(&m, 0, sizeof(m));
  m.VFS_LINK_LEN1 = strlen(name) + 1;
  m.VFS_LINK_LEN2 = strlen(name2) + 1;
  m.VFS_LINK_NAME1 = (char *) __UNCONST(name);
  m.VFS_LINK_NAME2 = (char *) __UNCONST(name2);
  return(_syscall(VFS_PROC_NR, VFS_RENAME, &m));
}
