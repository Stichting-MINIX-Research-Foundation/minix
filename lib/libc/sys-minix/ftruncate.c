#include <sys/cdefs.h>
#include "namespace.h"
#include <lib.h>

#include <minix/u64.h>
#include <string.h>
#include <unistd.h>

#ifdef __weak_alias
__weak_alias(ftruncate, _ftruncate)
#endif

int ftruncate(int _fd, off_t _length)
{
  message m;

  m.m2_l1 = ex64lo(_length);
  m.m2_l2 = ex64hi(_length);
  m.m2_i1 = _fd;

  return(_syscall(VFS_PROC_NR, FTRUNCATE, &m));
}
