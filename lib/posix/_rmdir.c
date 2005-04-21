#include <lib.h>
#define rmdir	_rmdir
#include <unistd.h>

PUBLIC int rmdir(name)
_CONST char *name;
{
  message m;

  _loadname(name, &m);
  return(_syscall(FS, RMDIR, &m));
}
