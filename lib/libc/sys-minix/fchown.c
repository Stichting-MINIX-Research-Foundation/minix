#include <sys/cdefs.h>
#include "namespace.h"
#include <lib.h>

#include <string.h>
#include <unistd.h>

#ifdef __weak_alias
__weak_alias(fchown, _fchown)
#endif

int fchown(int fd, uid_t owner, gid_t grp)
{
  message m;

  memset(&m, 0, sizeof(m));
  m.VFS_CHOWN_FD = fd;
  m.VFS_CHOWN_OWNER = owner;
  m.VFS_CHOWN_GROUP = grp;
  return(_syscall(VFS_PROC_NR, VFS_FCHOWN, &m));
}
