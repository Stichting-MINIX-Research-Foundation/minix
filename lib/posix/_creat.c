#include <lib.h>
#define creat	_creat
#include <fcntl.h>

PUBLIC int creat(name, mode)
_CONST char *name;
Mode_t mode;
{
  message m;

  m.m3_i2 = mode;
  _loadname(name, &m);
  return(_syscall(FS, CREAT, &m));
}
