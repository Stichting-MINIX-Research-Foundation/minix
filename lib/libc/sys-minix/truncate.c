#include <sys/cdefs.h>
#include "namespace.h"
#include <lib.h>

#include <minix/u64.h>
#include <string.h>
#include <unistd.h>

int truncate(const char *_path, off_t _length)
{
  message m;

  memset(&m, 0, sizeof(m));
  m.m_lc_vfs_truncate.name = (vir_bytes)_path;
  m.m_lc_vfs_truncate.len = strlen(_path)+1;
  m.m_lc_vfs_truncate.offset = _length;

  return(_syscall(VFS_PROC_NR, VFS_TRUNCATE, &m));
}
