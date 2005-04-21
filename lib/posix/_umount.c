#include <lib.h>
#define umount	_umount
#include <unistd.h>

PUBLIC int umount(name)
_CONST char *name;
{
  message m;

  _loadname(name, &m);
  return(_syscall(FS, UMOUNT, &m));
}
