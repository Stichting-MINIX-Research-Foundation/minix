#include <sys/cdefs.h>
#include "namespace.h"
#include <lib.h>

#include <string.h>
#include <unistd.h>

#ifdef __weak_alias
__weak_alias(getppid, _getppid)
#endif

pid_t getppid(void)
{
  message m;

  memset(&m, 0, sizeof(m));
  /* POSIX says that this function is always successful and that no
   * return value is reserved to indicate an error.  Minix syscalls
   * are not always successful and Minix returns the reserved value
   * (pid_t) -1 when there is an error.
   */
  if (_syscall(PM_PROC_NR, PM_GETPID, &m) < 0) return ( (pid_t) -1);
  return( (pid_t) m.PM_GETPID_PARENT);
}
