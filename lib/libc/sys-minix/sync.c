#include <sys/cdefs.h>
#include <lib.h>
#include "namespace.h"

#ifdef __weak_alias
__weak_alias(sync, _sync)
#endif

#include <unistd.h>

void sync()
{
  message m;

  (void)(_syscall(VFS_PROC_NR, SYNC, &m));
}
