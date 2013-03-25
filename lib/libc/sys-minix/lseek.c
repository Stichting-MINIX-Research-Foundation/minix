#include <sys/cdefs.h>
#include "namespace.h"
#include <lib.h>

#include <minix/u64.h>
#include <unistd.h>

#ifdef __weak_alias
__weak_alias(lseek, _lseek)
#endif

off_t
lseek(int fd, off_t offset, int whence)
{
  message m;

  m.m2_i1 = fd;
  m.m2_l1 = ex64lo(offset);
  m.m2_l2 = ex64hi(offset);
  m.m2_i2 = whence;

  if (_syscall(VFS_PROC_NR, LSEEK, &m) < 0)
	return( (off_t) -1);

  return( (off_t) make64(m.m2_l1, m.m2_l2));
}
