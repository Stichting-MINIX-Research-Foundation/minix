#include <sys/cdefs.h>
#include <lib.h>
#include "namespace.h"

#include <sys/stat.h>

#ifdef __weak_alias
__weak_alias(umask, _umask)
#endif

mode_t umask(mode_t complmode)
{
  message m;

  m.m1_i1 = complmode;
  return( (mode_t) _syscall(VFS_PROC_NR, UMASK, &m));
}
