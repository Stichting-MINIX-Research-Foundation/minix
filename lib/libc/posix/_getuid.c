#include <lib.h>
#define getuid	_getuid
#include <unistd.h>

PUBLIC uid_t getuid()
{
  message m;

  return( (uid_t) _syscall(MM, GETUID, &m));
}
