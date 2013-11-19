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

  memset(&m, 0, sizeof(m));
  m.VFS_TRUNCATE_OFF_LO = ex64lo(_length);
  m.VFS_TRUNCATE_OFF_HI = ex64hi(_length);
  m.VFS_TRUNCATE_FD = _fd;

  return(_syscall(VFS_PROC_NR, VFS_FTRUNCATE, &m));
}
