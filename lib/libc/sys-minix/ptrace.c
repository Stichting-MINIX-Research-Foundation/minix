#include <sys/cdefs.h>
#include "namespace.h"
#include <lib.h>

#include <string.h>
#include <sys/ptrace.h>

#ifdef __weak_alias
__weak_alias(ptrace, _ptrace)
#endif

int ptrace(int req, pid_t pid, void *addr, int data)
{
  message m;

  memset(&m, 0, sizeof(m));
  m.PM_PTRACE_PID = pid;
  m.PM_PTRACE_REQ = req;
  m.PM_PTRACE_ADDR = addr;
  m.PM_PTRACE_DATA = data;
  if (_syscall(PM_PROC_NR, PM_PTRACE, &m) < 0) return(-1);

  /* There was no error, but -1 is a legal return value.  Clear errno if
   * necessary to distinguish this case.  _syscall has set errno to nonzero
   * for the error case.
   */
  if (m.PM_PTRACE_DATA == -1) errno = 0;
  return(m.PM_PTRACE_DATA);
}
