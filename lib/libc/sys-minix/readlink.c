#include <sys/cdefs.h>
#include <lib.h>
#include "namespace.h"

#include <unistd.h>
#include <string.h>

#ifdef __weak_alias
__weak_alias(readlink, _readlink)
#endif

ssize_t readlink(const char *name, char *buffer, size_t bufsiz)
{
  message m;

  m.m1_i1 = strlen(name) + 1;
  m.m1_i2 = bufsiz;
  m.m1_p1 = (char *) __UNCONST(name);
  m.m1_p2 = (char *) buffer;

  return(_syscall(VFS_PROC_NR, RDLNK, &m));
}
