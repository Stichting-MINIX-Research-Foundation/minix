#include <lib.h>
#define getegid	_getegid
#include <unistd.h>

PUBLIC gid_t getegid()
{
  message m;

  /* POSIX says that this function is always successful and that no
   * return value is reserved to indicate an error.  Minix syscalls
   * are not always successful and Minix returns the unreserved value
   * (gid_t) -1 when there is an error.
   */
  if (_syscall(MM, GETGID, &m) < 0) return ( (gid_t) -1);
  return( (gid_t) m.m2_i1);
}
