#include <lib.h>
#define sigpending _sigpending
#include <signal.h>

PUBLIC int sigpending(set)
sigset_t *set;
{
  message m;

  if (_syscall(MM, SIGPENDING, &m) < 0) return(-1);
  *set = (sigset_t) m.m2_l1;
  return(m.m_type);
}
