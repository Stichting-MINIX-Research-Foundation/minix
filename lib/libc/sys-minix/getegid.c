#include <sys/cdefs.h>
#include "namespace.h"
#include <lib.h>

#include <unistd.h>

#ifdef __weak_alias
__weak_alias(getegid, _getegid)
#endif

gid_t getegid()
{
  message m;

  /* POSIX says that this function is always successful and that no
   * return value is reserved to indicate an error.  Minix syscalls
   * are not always successful and Minix returns the unreserved value
   * (gid_t) -1 when there is an error.
   */
  if (_syscall(PM_PROC_NR, GETGID, &m) < 0) return ( (gid_t) -1);
  return( (gid_t) m.m2_i1);
}
