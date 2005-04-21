#include <lib.h>
#define getpid	_getpid
#include <unistd.h>

PUBLIC pid_t getpid()
{
  message m;

  return(_syscall(MM, GETPID, &m));
}
