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

  memset(&m, 0, sizeof(m));
  m.VFS_TRUNCATE_NAME = (char *) __UNCONST(_path);
  m.VFS_TRUNCATE_LEN = strlen(_path)+1;
  m.VFS_TRUNCATE_OFF_LO = _length;

  return(_syscall(VFS_PROC_NR, VFS_TRUNCATE, &m));
}
