#include <sys/cdefs.h>
#include <lib.h>
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
	m.PM_SIG_HOW = SIG_INQUIRE;
	m.PM_SIG_SET = 0;
  } else {
	m.PM_SIG_HOW = how;
	m.PM_SIG_SET = (long) *set;
  }
  if (_syscall(PM_PROC_NR, PM_SIGPROCMASK, &m) < 0) return(-1);
  if (oset != (sigset_t *) NULL) *oset = (sigset_t) (m.PM_SIG_SET);
  return(m.m_type);
}

#if defined(__minix) && defined(__weak_alias)
__weak_alias(sigprocmask, __sigprocmask14)
#endif
