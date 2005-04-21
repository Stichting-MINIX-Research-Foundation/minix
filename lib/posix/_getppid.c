#include <lib.h>
#define getppid	_getppid
#include <unistd.h>

PUBLIC pid_t getppid()
{
  message m;

  /* POSIX says that this function is always successful and that no
   * return value is reserved to indicate an error.  Minix syscalls
   * are not always successful and Minix returns the reserved value
   * (pid_t) -1 when there is an error.
   */
  if (_syscall(MM, GETPID, &m) < 0) return ( (pid_t) -1);
  return( (pid_t) m.m2_i1);
}
