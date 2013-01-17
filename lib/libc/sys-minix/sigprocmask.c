#include <sys/cdefs.h>
#include <lib.h>
#include "namespace.h"

#include <signal.h>

int sigprocmask(how, set, oset)
int how;
const sigset_t *set;
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
  if (_syscall(PM_PROC_NR, SIGPROCMASK, &m) < 0) return(-1);
  if (oset != (sigset_t *) NULL) *oset = (sigset_t) (m.m2_l1);
  return(m.m_type);
}

#if defined(__minix) && defined(__weak_alias)
__weak_alias(sigprocmask, __sigprocmask14)
#endif
