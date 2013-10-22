#include <sys/cdefs.h>
#include <sys/signal.h>
#include <sys/sigtypes.h>
#include <lib.h>
#include <string.h>
#include "namespace.h"

#include <signal.h>

int sigprocmask(how, set, oset)
int how;
const sigset_t *set;
sigset_t *oset;
{
  message m;

  memset(&m, 0, sizeof(m));

  if (set == (sigset_t *) NULL) {
	m.SIG_HOW = SIG_INQUIRE;
  } else {
	m.SIG_HOW = how;
	memcpy(&m.SIG_MAP, set, sizeof(*set));
  }
  if (_syscall(PM_PROC_NR, SIGPROCMASK, &m) < 0) return(-1);
  if (oset != (sigset_t *) NULL) {
	memcpy(oset, &m.SIG_MAP, sizeof(*oset));
  }
  return(m.m_type);
}

#if defined(__minix) && defined(__weak_alias)
__weak_alias(sigprocmask, __sigprocmask14)
#endif
