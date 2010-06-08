#include <lib.h>
#define pause	_pause
#include <unistd.h>

PUBLIC int pause()
{
  message m;

  return(_syscall(PM_PROC_NR, PAUSE, &m));
}
