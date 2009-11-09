#include <lib.h>
#define unlink	_unlink
#include <unistd.h>

PUBLIC int unlink(name)
_CONST char *name;
{
  message m;

  _loadname(name, &m);
  return(_syscall(FS, UNLINK, &m));
}
