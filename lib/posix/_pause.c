#include <lib.h>
#define pause	_pause
#include <unistd.h>

PUBLIC int pause()
{
  message m;

  return(_syscall(MM, PAUSE, &m));
}
