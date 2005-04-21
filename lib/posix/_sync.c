#include <lib.h>
#define sync	_sync
#include <unistd.h>

PUBLIC int sync()
{
  message m;

  return(_syscall(FS, SYNC, &m));
}
