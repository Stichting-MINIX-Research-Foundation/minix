#include <sys/cdefs.h>
#include "namespace.h"
#include <lib.h>

#include <sys/ptrace.h>

#ifdef __weak_alias
__weak_alias(ptrace, _ptrace)
#endif

long ptrace(int req, pid_t pid, long addr, long data)
{
  message m;

  m.m2_i1 = pid;
  m.m2_i2 = req;
  m.PMTRACE_ADDR = addr;
  m.m2_l2 = data;
  if (_syscall(PM_PROC_NR, PTRACE, &m) < 0) return(-1);

  /* There was no error, but -1 is a legal return value.  Clear errno if
   * necessary to distinguish this case.  _syscall has set errno to nonzero
   * for the error case.
   */
  if (m.m2_l2 == -1) errno = 0;
  return(m.m2_l2);
}
