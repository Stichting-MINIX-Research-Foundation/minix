#include <lib.h>
#define setsid	_setsid
#include <unistd.h>

PUBLIC pid_t setsid()
{
  message m;

  return(_syscall(PM_PROC_NR, SETSID, &m));
}
