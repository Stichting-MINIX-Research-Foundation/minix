#include <sys/cdefs.h>
#include <lib.h>
#include "namespace.h"

#include <string.h>
#include <sys/stat.h>

#ifdef __weak_alias
__weak_alias(umask, _umask)
#endif

mode_t umask(mode_t complmode)
{
  message m;

  memset(&m, 0, sizeof(m));
  m.VFS_UMASK_MASK = complmode;
  return( (mode_t) _syscall(VFS_PROC_NR, VFS_UMASK, &m));
}
