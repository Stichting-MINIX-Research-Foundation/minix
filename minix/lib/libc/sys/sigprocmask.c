#include <sys/cdefs.h>
#include <sys/types.h>
#include <sys/sigtypes.h>
#include <sys/signal.h>
#include <lib.h>
#include <string.h>
#include "namespace.h"

#include <errno.h>
#include <string.h>
#include <signal.h>

/*
 * signalfd(2) support (see signalfd.c).  A signal in an active signalfd
 * mask is "owned": libc catches it with a handler that turns it into a
 * record on the signalfd.  Applications use signalfd as on Linux, i.e. they
 * also BLOCK the signal with sigprocmask() - but a blocked signal would
 * never reach the handler.  So owned signals are never really blocked: the
 * blocked state the application asked for is kept in 'sfd_shadow' and
 * merged back into every mask reported to it.  Before exec the shadow is
 * applied for real (exec resets the handler; Linux keeps them blocked).
 */
static sigset_t sfd_owned;	/* signals owned by a signalfd */
static sigset_t sfd_shadow;	/* owned signals the application blocked */
static int sfd_any;		/* sfd_owned is not empty */
static void (*sfd_handler)(int);	/* signalfd.c's catching handler */

int __libc_sigprocmask_raw(int how, const sigset_t *set, sigset_t *oset);

/* The real PM call, without signalfd adjustments: for libc-internal
 * critical sections (signalfd.c, timerfd.c) that must really block. */
int
__libc_sigprocmask_raw(int how, const sigset_t *set, sigset_t *oset)
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

/* Split an application mask into the real one and the shadow. */
static void
sfd_split(const sigset_t *app, sigset_t *real, sigset_t *shadow)
{
  int s;

  *real = *app;
  sigemptyset(shadow);
  for (s = 1; s < _NSIG; s++) {
	if (sigismember(&sfd_owned, s) > 0 && sigismember(app, s) > 0) {
		sigdelset(real, s);
		sigaddset(shadow, s);
	}
  }
}

static void
sfd_merge(sigset_t *set)
{
  int s;

  for (s = 1; s < _NSIG; s++)
	if (sigismember(&sfd_shadow, s) > 0)
		sigaddset(set, s);
}

int sigprocmask(how, set, oset)
int how;
const sigset_t *set;
sigset_t *oset;
{
  sigset_t real_old, app, real, shadow;
  int s;

  if (!sfd_any)
	return __libc_sigprocmask_raw(how, set, oset);

  if (__libc_sigprocmask_raw(SIG_BLOCK, NULL, &real_old) < 0) return(-1);
  app = real_old;
  sfd_merge(&app);
  if (oset != NULL) *oset = app;
  if (set == NULL) return(0);

  switch (how) {
  case SIG_BLOCK:
	for (s = 1; s < _NSIG; s++)
		if (sigismember(set, s) > 0) sigaddset(&app, s);
	break;
  case SIG_UNBLOCK:
	for (s = 1; s < _NSIG; s++)
		if (sigismember(set, s) > 0) sigdelset(&app, s);
	break;
  case SIG_SETMASK:
	app = *set;
	break;
  default:
	errno = EINVAL;
	return(-1);
  }
  sfd_split(&app, &real, &shadow);
  sfd_shadow = shadow;
  return __libc_sigprocmask_raw(SIG_SETMASK, &real, NULL);
}

/* signalfd.c: 'mask' is now (also) owned by a signalfd.  Owned signals that
 * are currently blocked move to the shadow and get unblocked for real, so
 * pending ones are delivered (into the signalfd) right away. */
void
__libc_sfd_own(const sigset_t *mask, void (*handler)(int))
{
  sigset_t app;
  int s;

  sfd_handler = handler;
  (void) sigprocmask(SIG_BLOCK, NULL, &app);	/* application view */
  for (s = 1; s < _NSIG; s++)
	if (sigismember(mask, s) > 0) {
		sigaddset(&sfd_owned, s);
		sfd_any = 1;
	}
  (void) sigprocmask(SIG_SETMASK, &app, NULL);
}

/* sigaction.c: a handler was installed for 'sig'.  If it is not the
 * signalfd handler, the signal is no longer owned: if the application has
 * it blocked, block it for real again. */
void
__libc_sfd_sigaction(int sig, void (*handler)(int))
{
  sigset_t one;

  if (!sfd_any || handler == sfd_handler || sigismember(&sfd_owned, sig) <= 0)
	return;
  sigdelset(&sfd_owned, sig);
  if (sigismember(&sfd_shadow, sig) > 0) {
	sigdelset(&sfd_shadow, sig);
	sigemptyset(&one);
	sigaddset(&one, sig);
	(void) __libc_sigprocmask_raw(SIG_BLOCK, &one, NULL);
  }
}

/* sigsuspend.c: the mask to really wait with. */
void
__libc_sfd_suspend_mask(const sigset_t *app, sigset_t *real)
{
  sigset_t shadow;

  if (!sfd_any) {
	*real = *app;
	return;
  }
  sfd_split(app, real, &shadow);
}

/* execve.c: before exec, block the shadow-blocked signals for real (the
 * handler is gone after exec); undo if exec fails. */
void
__libc_sfd_preexec(sigset_t *saved, int *changed)
{
  sigset_t real;

  *changed = 0;
  if (!sfd_any) return;
  if (__libc_sigprocmask_raw(SIG_BLOCK, NULL, saved) < 0) return;
  real = *saved;
  sfd_merge(&real);
  if (__libc_sigprocmask_raw(SIG_SETMASK, &real, NULL) == 0) *changed = 1;
}

void
__libc_sfd_postexec(const sigset_t *saved, int changed)
{
  if (changed) (void) __libc_sigprocmask_raw(SIG_SETMASK, saved, NULL);
}

/* pthread_sigmask(3) (<signal.h> maps it here): MINIX has no kernel
 * threads, so it is the process signal mask; errors are returned, not set. */
int __libc_thr_sigsetmask(int, const sigset_t * __restrict,
	sigset_t * __restrict);

int
__libc_thr_sigsetmask(int how, const sigset_t * __restrict set,
	sigset_t * __restrict oset)
{
  return sigprocmask(how, set, oset) == 0 ? 0 : errno;
}

#if defined(__minix) && defined(__weak_alias)
__weak_alias(sigprocmask, __sigprocmask14)
#endif
