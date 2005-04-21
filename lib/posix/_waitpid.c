#include <lib.h>
#define waitpid	_waitpid
#include <sys/wait.h>

PUBLIC pid_t waitpid(pid, status, options)
pid_t pid;
int *status;
int options;
{
  message m;

  m.m1_i1 = pid;
  m.m1_i2 = options;
  if (_syscall(MM, WAITPID, &m) < 0) return(-1);
  if (status != 0) *status = m.m2_i1;
  return m.m_type;
}
