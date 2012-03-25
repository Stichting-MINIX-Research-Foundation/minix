#include <sys/cdefs.h>
#include "namespace.h"
#include <lib.h>

#include <sys/stat.h>
#include <unistd.h>

#ifdef __weak_alias
__weak_alias(mkfifo, _mkfifo)
#endif

int mkfifo(const char *name, mode_t mode)
{
  return mknod(name, mode | S_IFIFO, (dev_t) 0);
}
