#include <sys/cdefs.h>
#include "namespace.h"
#include <lib.h>

#include <string.h>
#include <sys/ptrace.h>

int ptrace(int req, pid_t pid, void *addr, int data)
{
  message m;

  memset(&m, 0, sizeof(m));
  m.m_lc_pm_ptrace.pid = pid;
  m.m_lc_pm_ptrace.req = req;
  m.m_lc_pm_ptrace.addr = (vir_bytes)addr;
  m.m_lc_pm_ptrace.data = data;
  if (_syscall(PM_PROC_NR, PM_PTRACE, &m) < 0) return(-1);

  /* There was no error, but -1 is a legal return value.  Clear errno if
   * necessary to distinguish this case.  _syscall has set errno to nonzero
   * for the error case.
   */
  if (m.m_pm_lc_ptrace.data == -1) errno = 0;
  return(m.m_pm_lc_ptrace.data);
}
