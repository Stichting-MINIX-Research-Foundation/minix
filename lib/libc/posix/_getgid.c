#include <lib.h>
#define getgid	_getgid
#include <unistd.h>

PUBLIC gid_t getgid()
{
  message m;

  return( (gid_t) _syscall(MM, GETGID, &m));
}
