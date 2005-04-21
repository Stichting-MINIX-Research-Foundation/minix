#include <lib.h>
#define ptrace	_ptrace
#include <unistd.h>

PUBLIC long ptrace(req, pid, addr, data)
int req;
pid_t pid;
long addr;
long data;
{
  message m;

  m.m2_i1 = pid;
  m.m2_i2 = req;
  m.m2_l1 = addr;
  m.m2_l2 = data;
  if (_syscall(MM, PTRACE, &m) < 0) return(-1);

  /* There was no error, but -1 is a legal return value.  Clear errno if
   * necessary to distinguish this case.  _syscall has set errno to nonzero
   * for the error case.
   */
  if (m.m2_l2 == -1) errno = 0;
  return(m.m2_l2);
}
