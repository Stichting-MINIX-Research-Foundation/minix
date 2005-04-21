/* SYSVR4 and ANSI compatible signal(2). */

#include <lib.h>
#define sigaction	_sigaction
#define sigemptyset	_sigemptyset
#include <signal.h>

PUBLIC sighandler_t signal(sig, disp)
int sig;			/* signal number */
sighandler_t disp;		/* signal handler, or SIG_DFL, or SIG_IGN */
{
  struct sigaction sa, osa;

  if (sig <= 0 || sig > _NSIG || sig == SIGKILL) {
	errno = EINVAL;
	return(SIG_ERR);
  }
  sigemptyset(&sa.sa_mask);

#ifdef WANT_UNRELIABLE_SIGNALS
  /* Allow the signal being handled to interrupt the signal handler. */
  sa.sa_flags = SA_NODEFER;

  /* When signal is caught, reset signal handler to SIG_DFL for all but
   * SIGILL and SIGTRAP.
   */
  if (sig != SIGILL && sig != SIGTRAP) sa.sa_flags |= SA_RESETHAND;
#else
  sa.sa_flags = 0;
#endif

  sa.sa_handler = disp;
  if (sigaction(sig, &sa, &osa) < 0) return(SIG_ERR);
  return(osa.sa_handler);
}
