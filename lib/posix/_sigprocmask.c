#include <lib.h>
#define sigprocmask _sigprocmask
#include <signal.h>

PUBLIC int sigprocmask(how, set, oset)
int how;
_CONST sigset_t *set;
sigset_t *oset;
{
  message m;

  if (set == (sigset_t *) NULL) {
	m.m2_i1 = SIG_INQUIRE;
	m.m2_l1 = 0;
  } else {
	m.m2_i1 = how;
	m.m2_l1 = (long) *set;
  }
  if (_syscall(MM, SIGPROCMASK, &m) < 0) return(-1);
  if (oset != (sigset_t *) NULL) *oset = (sigset_t) (m.m2_l1);
  return(m.m_type);
}
