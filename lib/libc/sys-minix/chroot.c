#include <sys/cdefs.h>
#include "namespace.h"
#include <lib.h>

#include <unistd.h>

#ifdef __weak_alias
__weak_alias(chroot, _chroot)
#endif

int chroot(name)
const char *name;
{
  message m;

  _loadname(name, &m);
  return(_syscall(VFS_PROC_NR, CHROOT, &m));
}
