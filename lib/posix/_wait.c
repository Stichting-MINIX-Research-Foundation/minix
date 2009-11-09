#include <lib.h>
#define wait	_wait
#include <sys/wait.h>

PUBLIC pid_t wait(status)
int *status;
{
  message m;

  if (_syscall(MM, WAIT, &m) < 0) return(-1);
  if (status != 0) *status = m.m2_i1;
  return(m.m_type);
}
