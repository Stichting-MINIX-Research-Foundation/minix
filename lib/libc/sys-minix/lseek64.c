#include <sys/cdefs.h>
#include "namespace.h"
#include <lib.h>

#include <string.h>
#include <unistd.h>
#include <minix/u64.h>

int lseek64(fd, offset, whence, newpos)
int fd;
u64_t offset;
int whence;
u64_t *newpos;
{
  message m;

  memset(&m, 0, sizeof(m));
  m.VFS_LSEEK_FD = fd;
  m.VFS_LSEEK_OFF_LO = ex64lo(offset);
  m.VFS_LSEEK_OFF_HI = ex64hi(offset);
  m.VFS_LSEEK_WHENCE = whence;
  if (_syscall(VFS_PROC_NR, VFS_LSEEK, &m) < 0) return -1;
  if (newpos)
	*newpos= make64(m.VFS_LSEEK_OFF_LO, m.VFS_LSEEK_OFF_HI);
  return 0;
}
