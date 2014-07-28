/*	asyn_read()					Author: Kees J. Bot
 *								7 Jul 1997
 */
#include "asyn.h"
#include <signal.h>

ssize_t asyn_read(asynchio_t *asyn, int fd, void *buf, size_t len)
/* Asynchronous read().  Try if a read can be done, if not then set a flag
 * indicating that select(2) should look out for it.  Returns like a normal
 * read or returns -1 with errno set to EAGAIN.
 */
{
	asynfd_t *afd;

	/* Asyn_wait() may block if this counter equals zero indicating that
	 * all of the asyn_* functions are "in progress".
	 */
	asyn->asyn_more++;

	if ((unsigned) fd >= FD_SETSIZE) { errno= EBADF; return -1; }
	afd= &asyn->asyn_afd[fd];

	/* If this is the first async call on this filedescriptor then
	 * remember its file flags.
	 */
	if (!afd->afd_seen) {
		if ((afd->afd_flags= fcntl(fd, F_GETFL)) < 0) return -1;
		afd->afd_seen= 1;
	}

	/* Try to read if I/O is pending. */
	if (afd->afd_state[SEL_READ] == PENDING) {
		sigset_t mask;
		ssize_t result;
		int err;

		sigemptyset(&mask);
		if (sigprocmask(SIG_SETMASK, &mask, &mask) < 0) return -1;
		(void) fcntl(fd, F_SETFL, afd->afd_flags | O_NONBLOCK);

		/* Try the actual read. */
		result= read(fd, buf, len);
		err= errno;

		(void) fcntl(fd, F_SETFL, afd->afd_flags);
		(void) sigprocmask(SIG_SETMASK, &mask, nil);

		errno= err;
		if (result != -1 || errno != EAGAIN) {
			afd->afd_state[SEL_READ]= IDLE;
			return result;
		}
	}

	/* Record this read as "waiting". */
	afd->afd_state[SEL_READ]= WAITING;
	FD_SET(fd, &asyn->asyn_fdset[SEL_READ]);
	errno= EAGAIN;
	asyn->asyn_more--;
	return -1;
}
