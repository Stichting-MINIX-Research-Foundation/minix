#include <sys/cdefs.h>
#include "namespace.h"
#include <lib.h>

#include <unistd.h>

#ifdef __weak_alias
__weak_alias(geteuid, _geteuid)
#endif

uid_t geteuid()
{
  message m;

  /* POSIX says that this function is always successful and that no
   * return value is reserved to indicate an error.  Minix syscalls
   * are not always successful and Minix returns the unreserved value
   * (uid_t) -1 when there is an error.
   */
  if (_syscall(PM_PROC_NR, GETUID, &m) < 0) return ( (uid_t) -1);
  return( (uid_t) m.m2_i1);
}
