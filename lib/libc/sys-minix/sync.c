#include <sys/cdefs.h>
#include <lib.h>
#include "namespace.h"

#include <string.h>

#include <unistd.h>

void sync(void)
{
  message m;

  memset(&m, 0, sizeof(m));
  (void)(_syscall(VFS_PROC_NR, VFS_SYNC, &m));
}
