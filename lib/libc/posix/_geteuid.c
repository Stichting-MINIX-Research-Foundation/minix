#include <lib.h>
#define geteuid	_geteuid
#include <unistd.h>

PUBLIC uid_t geteuid()
{
  message m;

  /* POSIX says that this function is always successful and that no
   * return value is reserved to indicate an error.  Minix syscalls
   * are not always successful and Minix returns the unreserved value
   * (uid_t) -1 when there is an error.
   */
  if (_syscall(MM, GETUID, &m) < 0) return ( (uid_t) -1);
  return( (uid_t) m.m2_i1);
}
