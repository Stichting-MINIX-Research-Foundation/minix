#include <lib.h>
/* XXX - these have to be hidden because signal() uses them and signal() is
 * ANSI and not POSIX.  It would be surely be better to use macros for the
 * library and system uses, and perhaps macros as well as functions for the
 * POSIX user interface.  The macros would not need underlines.  It may be
 * inconvenient to match the exact semantics of the current functions
 * because the interface is bloated by reporting errors.  For library and
 * system uses, the signal number is mostly already known to be valid
 * before the sigset-changing routines are called.
 */
#define sigaddset	_sigaddset
#define sigdelset	_sigdelset
#define sigemptyset	_sigemptyset
#define sigfillset	_sigfillset
#define sigismember	_sigismember
#include <signal.h>

/* Low bit of signal masks. */
#define SIGBIT_0	((sigset_t) 1)

/* Mask of valid signals (0 - _NSIG). */
#define SIGMASK		(((SIGBIT_0 << _NSIG) << 1) - 1)

#define sigisvalid(signo) ((unsigned) (signo) <= _NSIG)

PUBLIC int sigaddset(set, signo)
sigset_t *set;
int signo;
{
  if (!sigisvalid(signo)) {
  	errno = EINVAL;
	return -1;
  }
  *set |= SIGBIT_0 << signo;
  return 0;
}

PUBLIC int sigdelset(set, signo)
sigset_t *set;
int signo;
{
  if (!sigisvalid(signo)) {
  	errno = EINVAL;
	return -1;
  }
  *set &= ~(SIGBIT_0 << signo);
  return 0;
}

PUBLIC int sigemptyset(set)
sigset_t *set;
{
  *set = 0;
  return 0;
}

PUBLIC int sigfillset(set)
sigset_t *set;
{
  *set = SIGMASK;
  return 0;
}

PUBLIC int sigismember(set, signo)
_CONST sigset_t *set;
int signo;
{
  if (!sigisvalid(signo)) {
  	errno = EINVAL;
	return -1;
  }
  if (*set & (SIGBIT_0 << signo))
  	return 1;
  return 0;
}
