/* signalfd.c - Linux-compatible signalfd for MINIX.
 *
 * Substrate: one pipe per signalfd (read end returned).  Signals in the
 * mask are caught by a libc-internal handler (SA_RESTART) which writes a
 * 128-byte signalfd_siginfo into the pipe (async-signal-safe write,
 * nonblocking; records may be dropped if the pipe is full).  Unlike Linux -
 * which has pending-signal retrieval - "blocked here" means consumed by the
 * handler: from the application's view the effect is the same (no default
 * action; event visible on the fd).
 *
 * signalfd(2) on an existing fd updates the mask.  The libc-held write end
 * is F_SETNOSIGPIPE, so once the signalfd is closed, handler writes fail
 * with a silent EPIPE rather than raising SIGPIPE.
 */

#include <sys/cdefs.h>
#include "namespace.h"

#include <sys/signalfd.h>
#include <errno.h>
#include <fcntl.h>
#include <signal.h>
#include <stdint.h>
#include <string.h>
#include <unistd.h>

#define MAX_SIGFDS 16

static struct sfd {
	int		rfd;
	int		wfd;
	sigset_t	mask;
} sfds[MAX_SIGFDS];
static int nsfds;

static void
sfd_handler(int signo)
{
	struct signalfd_siginfo ssi;
	int i, r;

	memset(&ssi, 0, sizeof(ssi));
	ssi.ssi_signo = signo;
	ssi.ssi_code = SI_USER;

	for (i = 0; i < nsfds; i++)
		if (sigismember(&sfds[i].mask, signo) > 0) {
			r = write(sfds[i].wfd, &ssi, sizeof(ssi));
			(void)r;	/* drop on EAGAIN/EPIPE */
		}
}

/* sigprocmask.c */
void __libc_sfd_own(const sigset_t *mask, void (*handler)(int));
int __libc_sigprocmask_raw(int how, const sigset_t *set, sigset_t *oset);

static int
sfd_arm_mask(const sigset_t *mask)
{
	/* Install catching handlers for everything in the mask.  Unlike Linux
	 * we do not just block the signals (there is no pending-siginfo
	 * retrieval API); instead the handler consumes the signal into the
	 * pipe, so for the application the effect is the same: no default
	 * action, event visible via the fd.  SA_RESTART keeps poll(2)
	 * seamless.  (KILL/STOP can't be handled this way - reject.) */
	struct sigaction sa;
	int signo;

	memset(&sa, 0, sizeof(sa));
	sa.sa_handler = sfd_handler;
	sa.sa_flags = SA_RESTART;
	sigemptyset(&sa.sa_mask);

	for (signo = 1; signo < _NSIG; signo++) {
		if (sigismember(mask, signo) > 0) {
			if (signo == SIGKILL || signo == SIGSTOP)
				return EINVAL;
			if (sigaction(signo, &sa, NULL) != 0)
				return errno;
		}
	}
	/* Applications block signalfd signals (as on Linux); make sure that
	 * never keeps them from our handler. */
	__libc_sfd_own(mask, sfd_handler);
	return 0;
}

static int
sfd_find(int fd)
{
	int i;
	for (i = 0; i < nsfds; i++)
		if (sfds[i].rfd == fd) return i;
	return -1;
}

int
signalfd(int fd, const sigset_t *mask, int flags)
{
	int r, i;

	if (mask == NULL) { errno = EINVAL; return -1; }

	if (fd != -1) {
		/* update existing instance */
		if ((i = sfd_find(fd)) < 0) { errno = EBADF; return -1; }
		if (flags != 0) { errno = EINVAL; return -1; }
		if ((r = sfd_arm_mask(mask)) != 0) { errno = r; return -1; }
		sfds[i].mask = *mask;
		return fd;
	}

	{
		int fds[2];
		sigset_t all, omask;
		if (pipe(fds) != 0) return -1;
		/* sfd_handler walks sfds[]: keep it out while we edit */
		sigfillset(&all);
		__libc_sigprocmask_raw(SIG_BLOCK, &all, &omask);
		/* fd numbers are recycled after close(2): drop an entry of a
		 * closed signalfd that names our new read end (its write end
		 * is still open, so cannot collide with the new pipe). */
		for (i = 0; i < nsfds; i++) {
			if (sfds[i].rfd == fds[0]) {
				close(sfds[i].wfd);
				sfds[i] = sfds[--nsfds];
				break;
			}
		}
		if (nsfds >= MAX_SIGFDS) {
			__libc_sigprocmask_raw(SIG_SETMASK, &omask, NULL);
			close(fds[0]);
			close(fds[1]);
			errno = ENFILE;
			return -1;
		}
		i = nsfds++;
		sfds[i].rfd = fds[0];
		sfds[i].wfd = fds[1];
		sfds[i].mask = *mask;
		__libc_sigprocmask_raw(SIG_SETMASK, &omask, NULL);

		if (flags & SFD_NONBLOCK) {
			fcntl(fds[0], F_SETFL,
			    fcntl(fds[0], F_GETFL) | O_NONBLOCK);
		}
		if (flags & SFD_CLOEXEC)
			fcntl(fds[0], F_SETFD,
			    fcntl(fds[0], F_GETFD) | FD_CLOEXEC);
		/* write side is signal-handler context: never block; and it
		 * must never leak into exec'd programs */
		fcntl(fds[1], F_SETFL, fcntl(fds[1], F_GETFL) | O_NONBLOCK);
		fcntl(fds[1], F_SETFD, fcntl(fds[1], F_GETFD) | FD_CLOEXEC);
		/* Once the signalfd is closed, handler writes must fail
		 * quietly with EPIPE instead of raising SIGPIPE.  (Do not
		 * block SIGPIPE instead: the mask is process-wide and would
		 * be inherited by every program we exec.) */
		fcntl(fds[1], F_SETNOSIGPIPE, 1);
	}

	if ((r = sfd_arm_mask(mask)) != 0) {
		errno = r;
		close(sfds[i].rfd);
		close(sfds[i].wfd);
		nsfds--;
		return -1;
	}
	return sfds[i].rfd;
}
