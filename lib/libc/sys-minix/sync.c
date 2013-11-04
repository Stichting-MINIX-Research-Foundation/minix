#include <sys/cdefs.h>
#include <lib.h>
#include "namespace.h"

#include <string.h>

#ifdef __weak_alias
__weak_alias(sync, _sync)
#endif

#include <unistd.h>

void sync(void)
{
  message m;

  memset(&m, 0, sizeof(m));
  (void)(_syscall(VFS_PROC_NR, VFS_SYNC, &m));
}
