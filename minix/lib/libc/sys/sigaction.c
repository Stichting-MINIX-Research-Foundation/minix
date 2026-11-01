#include <sys/cdefs.h>
#include <lib.h>
#include "namespace.h"

#include <string.h>
#include <signal.h>

int __sigreturn(void);

/*
 * Track which signals are handled with SA_RESTART, so select(2) (and poll,
 * which rides on it) can restart like POSIX/BSD do.  The syscall layer
 * (VFS/PM) does not tell us SA_RESTART semantics, so we keep the bits here.
 * A signal whose handler is reset loses the bit (good enough: signal()
 * re-installs with SA_RESTART as usual on *BSD).
 */
static unsigned long sa_restart_map[(_NSIG + 8 * sizeof(long) - 1) /
    (8 * sizeof(long))];

/* for select.c */
int
__libc_select_may_restart(void)
{
	size_t i;
	for (i = 0; i < sizeof(sa_restart_map) / sizeof(sa_restart_map[0]); i++)
		if (sa_restart_map[i])
			return 1;
	return 0;
}

/* sigprocmask.c */
void __libc_sfd_sigaction(int sig, void (*handler)(int));

int sigaction(int sig, const struct sigaction *act, struct sigaction *oact)
{
  message m;
  int r;

  memset(&m, 0, sizeof(m));
  m.m_lc_pm_sig.nr = sig;
  m.m_lc_pm_sig.act = (vir_bytes)act;
  m.m_lc_pm_sig.oact = (vir_bytes)oact;
  m.m_lc_pm_sig.ret = (vir_bytes)__sigreturn;

  r = _syscall(PM_PROC_NR, PM_SIGACTION, &m);

  if (r >= 0 && act != NULL)
	__libc_sfd_sigaction(sig, act->sa_handler);	/* signalfd ownership */
  if (r >= 0) {
	unsigned long mask = 1UL << ((sig - 1) % (8 * sizeof(long)));
	int idx = (sig - 1) / (8 * sizeof(long));
	if (act != NULL && (act->sa_flags & SA_RESTART) != 0 &&
	    act->sa_handler != SIG_IGN && act->sa_handler != SIG_DFL)
		sa_restart_map[idx] |= mask;
	else
		sa_restart_map[idx] &= ~mask;
  }
  return r;
}

#if defined(__minix) && defined(__weak_alias)
__weak_alias(sigaction, __sigaction14)
#endif
