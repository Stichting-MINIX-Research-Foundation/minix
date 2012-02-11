#include <sys/cdefs.h>
#include <lib.h>
#include "namespace.h"

#include <unistd.h>

#ifdef __weak_alias
__weak_alias(unlink, _unlink)
#endif

int unlink(name)
const char *name;
{
  message m;

  _loadname(name, &m);
  return(_syscall(VFS_PROC_NR, UNLINK, &m));
}
