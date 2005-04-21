#include <lib.h>
#define sigsuspend _sigsuspend
#include <signal.h>

PUBLIC int sigsuspend(set)
_CONST sigset_t *set;
{
  message m;

  m.m2_l1 = (long) *set;
  return(_syscall(MM, SIGSUSPEND, &m));
}
