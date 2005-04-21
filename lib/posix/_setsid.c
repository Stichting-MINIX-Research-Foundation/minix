#include <lib.h>
#define setsid	_setsid
#include <unistd.h>

PUBLIC pid_t setsid()
{
  message m;

  return(_syscall(MM, SETSID, &m));
}
