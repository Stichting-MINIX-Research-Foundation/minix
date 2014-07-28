#include <sys/cdefs.h>
#include <sys/types.h>
#include <sys/sigtypes.h>
#include <sys/signal.h>
#include <lib.h>
#include <string.h>
#include "namespace.h"

#include <string.h>
#include <signal.h>

int sigprocmask(how, set, oset)
int how;
const sigset_t *set;
sigset_t *oset;
{
  message m;

  memset(&m, 0, sizeof(m));
  if (set == (sigset_t *) NULL) {
	m.m_lc_pm_sigset.how = SIG_INQUIRE;
	sigemptyset(&m.m_lc_pm_sigset.set);
  } else {
	m.m_lc_pm_sigset.how = how;
	m.m_lc_pm_sigset.set = *set;
  }
  if (_syscall(PM_PROC_NR, PM_SIGPROCMASK, &m) < 0) return(-1);
  if (oset != NULL) *oset = m.m_pm_lc_sigset.set;

  return(m.m_type);
}

#if defined(__minix) && defined(__weak_alias)
__weak_alias(sigprocmask, __sigprocmask14)
#endif
