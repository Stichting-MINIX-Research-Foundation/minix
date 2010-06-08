#include <lib.h>
#define creat	_creat
#include <fcntl.h>

PUBLIC int creat(const char *name, mode_t mode)
{
  message m;

  m.m3_i2 = mode;
  _loadname(name, &m);
  return(_syscall(VFS_PROC_NR, CREAT, &m));
}
