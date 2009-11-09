#include <lib.h>
#define kill	_kill
#include <signal.h>

PUBLIC int kill(proc, sig)
int proc;			/* which process is to be sent the signal */
int sig;			/* signal number */
{
  message m;

  m.m1_i1 = proc;
  m.m1_i2 = sig;
  return(_syscall(MM, KILL, &m));
}
