#include <lib.h>
#define chmod	_chmod
#include <sys/stat.h>

PUBLIC int chmod(name, mode)
_CONST char *name;
Mode_t mode;
{
  message m;

  m.m3_i2 = mode;
  _loadname(name, &m);
  return(_syscall(FS, CHMOD, &m));
}
