#include <lib.h>
/* System processes use simpler macros with no range error checking (defined in
 * signal.h). The ANSI signal() implementation now also uses the macro
 * versions, which makes hiding of the functions here a historical remains.
 *
 * _NSIG is supposed to be the highest signal number plus one.
 */
#define sigaddset	_sigaddset
#define sigdelset	_sigdelset
#define sigemptyset	_sigemptyset
#define sigfillset	_sigfillset
#define sigismember	_sigismember
#include <signal.h>

/* Low bit of signal masks. */
#define SIGBIT_0	((sigset_t) 1)

/* Mask of valid signals (0 - (_NSIG-1)). */
#define SIGMASK		((SIGBIT_0 << _NSIG) - 1)

#define sigisvalid(signo) ((unsigned) (signo) < _NSIG)

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
