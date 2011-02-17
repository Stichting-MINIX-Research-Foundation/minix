#include <sys/cdefs.h>
#include <lib.h>
#include "namespace.h"

#ifdef __weak_alias
__weak_alias(rmdir, _rmdir)
#endif

#include <unistd.h>

int rmdir(name)
const char *name;
{
  message m;

  _loadname(name, &m);
  return(_syscall(VFS_PROC_NR, RMDIR, &m));
}
