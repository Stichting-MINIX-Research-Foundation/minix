#include <lib.h>
#define chroot	_chroot
#include <unistd.h>

PUBLIC int chroot(name)
_CONST char *name;
{
  message m;

  _loadname(name, &m);
  return(_syscall(FS, CHROOT, &m));
}
