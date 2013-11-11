#include <sys/cdefs.h>
#include "namespace.h"
#include <lib.h>

#include <unistd.h>

int lseek64(fd, offset, whence, newpos)
int fd;
u64_t offset;
int whence;
u64_t *newpos;
{
  message m;

  m.m2_i1 = fd;
  m.m2_l1 = (unsigned long)(offset);
  m.m2_l2 = (unsigned long)(offset>>32);
  m.m2_i2 = whence;
  if (_syscall(VFS_PROC_NR, LLSEEK, &m) < 0) return -1;
  if (newpos)
	*newpos= (u64_t)m.m2_l1 | ((u64_t)m.m2_l2<<32);
  return 0;
}
