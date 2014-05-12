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
  m.m_lc_vfs_truncate.offset = _length;
  m.m_lc_vfs_truncate.fd = _fd;

  return(_syscall(VFS_PROC_NR, VFS_FTRUNCATE, &m));
}
