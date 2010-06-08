#include <lib.h>
#define chmod	_chmod
#include <sys/stat.h>

PUBLIC int chmod(const char *name, mode_t mode)
{
  message m;

  m.m3_i2 = mode;
  _loadname(name, &m);
  return(_syscall(VFS_PROC_NR, CHMOD, &m));
}
