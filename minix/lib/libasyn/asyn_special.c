/*	asyn_special(), asyn_result()			Author: Kees J. Bot
 *								8 Jul 1997
 */
#include "asyn.h"
#include <signal.h>

/* Saved signal mask between asyn_special() and asyn_result(). */
static sigset_t mask;

int asyn_special(asynchio_t *asyn, int fd, int op)
/* Wait for an operation.  This is an odd one out compared to asyn_read()
 * and asyn_write().  It does not do an operation itself, but together with
 * asyn_result() it is a set of brackets around a system call xxx that has
 * no asyn_xxx() for itself.  It can be used to build an asyn_accept() or
 * asyn_connect() for instance.  (Minix-vmd has asyn_ioctl() instead,
 * which is used for any other event like TCP/IP listen/connect.  BSD has
 * a myriad of calls that can't be given an asyn_xxx() counterpart each.)
 * Asyn_special() returns -1 for "forget it", 0 for "try it", and 1 for
 * "very first call, maybe you should try it once, maybe not".  Errno is
 * set to EAGAIN if the result is -1 or 1.  After trying the system call
 * make sure errno equals EAGAIN if the call is still in progress and call
 * asyn_result with the result of the system call.  Asyn_result() must be
 * called if asyn_special() returns 0 or 1.
 *
 * Example use:
 *
 * int asyn_accept(asynchio_t *asyn, int s, struct sockaddr *addr, int *addrlen)
 * {
 *     int r;
 *     if ((r= asyn_special(asyn, fd, SEL_READ)) < 0) return -1;
 *     r= r == 0 ? accept(fd, addr, addrlen) : -1;
 *     return asyn_result(asyn, fd, SEL_READ, r);
 * }
 *
 * int asyn_connect(asynchio_t *asyn, int s, struct sockaddr *name, int namelen)
 * {
 *     int r;
 *     if ((r= asyn_special(asyn, fd, SEL_WRITE)) < 0) return -1;
 *     if (r == 1 && (r= connect(fd, name, namelen)) < 0) {
 *         if (errno == EINPROGRESS) errno= EAGAIN;
 *     }
 *     return asyn_result(asyn, fd, SEL_WRITE, r);
 * }
 */
{
	asynfd_t *afd;
	int seen;

	asyn->asyn_more++;

	if ((unsigned) fd >= FD_SETSIZE) { errno= EBADF; return -1; }
	afd= &asyn->asyn_afd[fd];

	/* If this is the first async call on this filedescriptor then
	 * remember its file flags.
	 */
	if (!(seen= afd->afd_seen)) {
		if ((afd->afd_flags= fcntl(fd, F_GETFL)) < 0) return -1;
		afd->afd_seen= 1;
	}

	/* Try to read if I/O is pending. */
	if (!seen || afd->afd_state[op] == PENDING) {
		sigemptyset(&mask);
		if (sigprocmask(SIG_SETMASK, &mask, &mask) < 0) return -1;
		(void) fcntl(fd, F_SETFL, afd->afd_flags | O_NONBLOCK);

		/* Let the caller try the system call. */
		errno= EAGAIN;
		return seen ? 0 : 1;
	}

	/* Record this read as "waiting". */
	afd->afd_state[op]= WAITING;
	FD_SET(fd, &asyn->asyn_fdset[op]);
	errno= EAGAIN;
	asyn->asyn_more--;
	return -1;
}

int asyn_result(asynchio_t *asyn, int fd, int op, int result)
/* The caller has tried the system call with the given result.  Finish up. */
{
	int err;
	asynfd_t *afd= &asyn->asyn_afd[fd];

	err= errno;

	(void) fcntl(fd, F_SETFL, afd->afd_flags);
	(void) sigprocmask(SIG_SETMASK, &mask, nil);

	errno= err;
	if (result != -1 || errno != EAGAIN) {
		afd->afd_state[op]= IDLE;
		return result;
	}

	/* Record this operation as "waiting". */
	afd->afd_state[op]= WAITING;
	FD_SET(fd, &asyn->asyn_fdset[op]);
	errno= EAGAIN;
	asyn->asyn_more--;
	return -1;
}
