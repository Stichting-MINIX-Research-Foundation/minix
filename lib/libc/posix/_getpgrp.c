#include <lib.h>
#define getpgrp	_getpgrp
#include <unistd.h>

PUBLIC pid_t getpgrp()
{
  message m;

  return(_syscall(PM_PROC_NR, GETPGRP, &m));
}
