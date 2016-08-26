/*	asyn_wait() - wait for asynch operations	Author: Kees J. Bot
 *								7 Jul 1997
 */

#include "asyn.h"
#include <time.h>
#ifdef DEBUG
#include <stdio.h>
#endif

#define TBOUND_MIN	1
#define TBOUND_MAX	16

int asyn_wait(asynchio_t *asyn, int flags, struct timeval *to)
/* Wait for one or more nonblocking operations to return a result. */
{
	int r;
	static struct timeval zero_time;
	struct timeval t;
	static time_t tbound= TBOUND_MIN;

	/* Are there more things to do before we can block? */
	if (asyn->asyn_more > 0) { asyn->asyn_more= 0; return 0; }

	if (flags & ASYN_NONBLOCK) {
		/* Don't block by using a zero second timeout. */
		to= &zero_time;
	} else
	if (to != nil) {
		/* asyn_wait() uses an absolute time. */
		if (to->tv_usec >= 1000000L) {
			to->tv_sec+= to->tv_usec / 1000000L;
			to->tv_usec%= 1000000L;
		}
		(void) gettimeofday(&t, nil);
		if (t.tv_sec > to->tv_sec || (t.tv_sec == to->tv_sec
						&& t.tv_usec >= to->tv_usec)) {
			to= &zero_time;
		} else {
			t.tv_sec= to->tv_sec - t.tv_sec;
			t.tv_usec= to->tv_usec - t.tv_usec;
			if (t.tv_usec < 0) {
				t.tv_sec--;
				t.tv_usec+= 1000000L;
			}
			to= &t;
		}

		/* Don't sleep too long, we don't trust select(). */
		if (to->tv_sec > tbound) goto bound;
	} else {
	bound:
		/* No timeout?  Don't hang in (buggy?) select() forever. */
		to= &t;
		t.tv_sec= tbound;
		t.tv_usec= 0;
	}

#ifdef DEBUG
	{
		int op;

		fprintf(stderr, "select: ");
		for (op= 0; op < SEL_NR; op++) {
			fd_set *fdsetp= &asyn->asyn_fdset[op];
			int fd;

			for (fd= 0; fd < FD_SETSIZE; fd++) {
				if (FD_ISSET(fd, fdsetp)) {
					asyn->asyn_afd[fd].afd_state[op]=
								PENDING;
					fprintf(stderr, "%d%c", fd, "rwx"[op]);
				}
			}
		}
		fflush(stderr);
	}
#endif
	r= select(FD_SETSIZE, &asyn->asyn_fdset[SEL_READ],
				&asyn->asyn_fdset[SEL_WRITE],
				&asyn->asyn_fdset[SEL_EXCEPT], to);
#ifdef DEBUG
	fprintf(stderr, " (%d) ", r);
#endif
	if (r > 0) {
		/* An event occurred on one or more file descriptors. */
		int op;

		for (op= 0; op < SEL_NR; op++) {
			fd_set *fdsetp= &asyn->asyn_fdset[op];
			int fd;

			for (fd= 0; fd < FD_SETSIZE; fd++) {
				if (FD_ISSET(fd, fdsetp)) {
					asyn->asyn_afd[fd].afd_state[op]=
								PENDING;
#ifdef DEBUG
					fprintf(stderr, "%d%c", fd, "rwx"[op]);
#endif
				}
			}
		}
		tbound= TBOUND_MIN;
	} else
	if (r == 0) {
		/* If nothing happened then let the time boundary slip a bit. */
		if (tbound < TBOUND_MAX) tbound <<= 1;
	}
#ifdef DEBUG
	fputc('\n', stderr);
#endif

	FD_ZERO(&asyn->asyn_fdset[SEL_READ]);
	FD_ZERO(&asyn->asyn_fdset[SEL_WRITE]);
	FD_ZERO(&asyn->asyn_fdset[SEL_EXCEPT]);

	return r == 0 ? (errno= EINTR, -1) : r;
}
