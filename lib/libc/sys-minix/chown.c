#include <sys/cdefs.h>
#include "namespace.h"
#include <lib.h>

#include <string.h>
#include <unistd.h>

#ifdef __weak_alias
__weak_alias(__posix_chown, chown)
#endif

int chown(const char *name, uid_t owner, gid_t grp)
{
  message m;

  memset(&m, 0, sizeof(m));
  m.VFS_CHOWN_LEN = strlen(name) + 1;
  m.VFS_CHOWN_OWNER = owner;
  m.VFS_CHOWN_GROUP = grp;
  m.VFS_CHOWN_NAME = (char *) __UNCONST(name);
  return(_syscall(VFS_PROC_NR, VFS_CHOWN, &m));
}
