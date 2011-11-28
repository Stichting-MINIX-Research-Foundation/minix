#include <sys/cdefs.h>
#include "namespace.h"
#include <lib.h>

#ifdef __weak_alias
__weak_alias(truncate, _truncate)
#endif

#include <string.h>
#include <unistd.h>


int truncate(const char *_path, off_t _length)
{
  message m;
  m.m2_p1 = (char *) __UNCONST(_path);
  m.m2_i1 = strlen(_path)+1;
  m.m2_l1 = _length;

  return(_syscall(VFS_PROC_NR, TRUNCATE, &m));
}
