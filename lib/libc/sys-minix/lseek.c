#include <sys/cdefs.h>
#include "namespace.h"
#include <lib.h>

#include <string.h>
#include <minix/u64.h>
#include <unistd.h>

#ifdef __weak_alias
__weak_alias(lseek, _lseek)
#endif

off_t
lseek(int fd, off_t offset, int whence)
{
  message m;

  memset(&m, 0, sizeof(m));
  m.VFS_LSEEK_FD = fd;
  m.VFS_LSEEK_OFF = offset;
  m.VFS_LSEEK_WHENCE = whence;
  if (_syscall(VFS_PROC_NR, VFS_LSEEK, &m) < 0) return( (off_t) -1);
  return( (off_t) m.VFS_LSEEK_OFF);
}
