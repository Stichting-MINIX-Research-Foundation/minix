#include <lib.h>
#define fork	_fork
#include <unistd.h>

PUBLIC pid_t fork()
{
  message m;

  return(_syscall(MM, FORK, &m));
}
