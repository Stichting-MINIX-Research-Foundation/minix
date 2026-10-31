/* timerfd.c - Linux-compatible timerfd for MINIX.
 *
 * Substrate: one pipe per timerfd (read end returned; the write end is
 * owned by libc and written by the SIGALRM handler).  One process-wide
 * setitimer(ITIMER_REAL) is multiplexed over all timerfds: the kernel timer
 * is (re)programmed to the earliest armed deadline; on expiry, all elapsed
 * timers get a unit count pushed and periodic ones are re-armed.
 *
 * Limitations (beyond Linux): clockid is advisory (CLOCK_MONOTONIC is used
 * when available, else REALTIME); applications must not use alarm(2) or
 * setitimer(2) themselves; timer counts are saturating per-pipe.
 */

#include <sys/cdefs.h>
#include "namespace.h"

#include <sys/timerfd.h>
#include <sys/time.h>
#include <errno.h>
#include <limits.h>
#include <signal.h>
#include <stdint.h>
#include <string.h>
#include <unistd.h>
#include <fcntl.h>

#define MAX_TIMERFDS 32

static struct tfd {
	int	rfd;		/* public (read) end */
	int	wfd;		/* libc-owned write end, nonblocking */
	clockid_t clockid;
	uint64_t next_ns;	/* absolute deadline, UINT64_MAX = disarmed */
	uint64_t period_ns;	/* 0 = oneshot */
} tfds[MAX_TIMERFDS];
static int ntfds;
static int tfd_installed;	/* SIGALRM handler installed */
static volatile sig_atomic_t tfd_inhandler;

/* The SIGALRM handler walks tfds[]: block it while the table (or a 64-bit
 * deadline, which i386 can't store atomically) is being changed. */
static void
tfd_lock(sigset_t *omask)
{
	sigset_t m;

	sigemptyset(&m);
	sigaddset(&m, SIGALRM);
	sigprocmask(SIG_BLOCK, &m, omask);
}

static void
tfd_unlock(const sigset_t *omask)
{
	sigprocmask(SIG_SETMASK, omask, NULL);
}

static uint64_t
now_ns(clockid_t clk)
{
	struct timespec ts;
	if (clock_gettime(clk, &ts) != 0)
		clock_gettime(CLOCK_REALTIME, &ts);
	return (uint64_t)ts.tv_sec * 1000000000ULL + ts.tv_nsec;
}

static void
tfd_rearm_itimer(void)
{
	/* program the process timer to the earliest armed deadline */
	uint64_t best = UINT64_MAX, now = 0;
	int i;

	for (i = 0; i < ntfds; i++)
		if (tfds[i].next_ns < best)
			best = tfds[i].next_ns;
	if (best == UINT64_MAX) {
		struct itimerval disarm;
		memset(&disarm, 0, sizeof(disarm));
		setitimer(ITIMER_REAL, &disarm, NULL);
		return;
	}
	now = now_ns(CLOCK_REALTIME);
	{
		uint64_t delta_ns = best > now ? best - now : 1;
		struct itimerval it;
		memset(&it, 0, sizeof(it));
		it.it_value.tv_sec = delta_ns / 1000000000ULL;
		it.it_value.tv_usec = (delta_ns % 1000000000ULL) / 1000;
		if (it.it_value.tv_sec == 0 && it.it_value.tv_usec == 0)
			it.it_value.tv_usec = 1;
		setitimer(ITIMER_REAL, &it, NULL);
	}
}

static void
tfd_sigalrm(int sig)
{
	static const uint64_t one = 1;
	uint64_t now;
	int i;
	(void)sig;

	if (tfd_inhandler) return;	/* paranoia */
	tfd_inhandler = 1;
	now = now_ns(CLOCK_REALTIME);
	for (i = 0; i < ntfds; i++) {
		if (tfds[i].next_ns != UINT64_MAX && tfds[i].next_ns <= now) {
			/* push one expiration; drop when pipe full */
			(void)write(tfds[i].wfd, &one, 8);
			if (tfds[i].period_ns)
				tfds[i].next_ns = now + tfds[i].period_ns;
			else
				tfds[i].next_ns = UINT64_MAX;
		}
	}
	tfd_inhandler = 0;
	tfd_rearm_itimer();
}

int
timerfd_create(int clockid, int flags)
{
	int fds[2];
	int i, r;
	struct sigaction sa;
	sigset_t omask;

	if (pipe(fds) != 0) return -1;
	tfd_lock(&omask);

	/* fd numbers are recycled after close(2): an entry still naming our
	 * new read end belongs to a closed timerfd.  Drop it (closing its
	 * libc-held write end, which is still open and so cannot collide
	 * with the new pipe), or lookups would find the stale entry first. */
	for (i = 0; i < ntfds; i++) {
		if (tfds[i].rfd == fds[0]) {
			close(tfds[i].wfd);
			tfds[i] = tfds[--ntfds];
			break;
		}
	}

	if (ntfds >= MAX_TIMERFDS) {
		tfd_unlock(&omask);
		r = ENFILE;
		goto fail;
	}

	/* read end returned to caller */
	i = ntfds++;
	tfds[i].rfd = fds[0];
	tfds[i].wfd = fds[1];
	tfds[i].clockid = clockid;
	tfds[i].next_ns = UINT64_MAX;
	tfds[i].period_ns = 0;
	tfd_unlock(&omask);

	if (flags & TFD_NONBLOCK)
		fcntl(fds[0], F_SETFL, fcntl(fds[0], F_GETFL) | O_NONBLOCK);
	if (flags & TFD_CLOEXEC)
		fcntl(fds[0], F_SETFD, fcntl(fds[0], F_GETFD) | FD_CLOEXEC);
	/* the write end is always nonblocking (signal handler context),
	 * must never leak into exec'd programs, and must not raise SIGPIPE
	 * once the timerfd is closed (the handler just drops the count) */
	fcntl(fds[1], F_SETFL, fcntl(fds[1], F_GETFL) | O_NONBLOCK);
	fcntl(fds[1], F_SETFD, fcntl(fds[1], F_GETFD) | FD_CLOEXEC);
	fcntl(fds[1], F_SETNOSIGPIPE, 1);

	if (!tfd_installed) {
		memset(&sa, 0, sizeof(sa));
		sa.sa_handler = tfd_sigalrm;
		sa.sa_flags = SA_RESTART;	/* keep poll(2) seamless */
		sigemptyset(&sa.sa_mask);
		if (sigaction(SIGALRM, &sa, NULL) != 0) {
			r = errno;
			ntfds--;	/* handler not installed: no race */
			close(fds[0]);
			close(fds[1]);
			errno = r;
			return -1;
		}
		tfd_installed = 1;
	}
	return fds[0];

fail:
	close(fds[0]);
	close(fds[1]);
	errno = r;
	return -1;
}

static int
tfd_find(int fd)
{
	int i;
	for (i = 0; i < ntfds; i++)
		if (tfds[i].rfd == fd) return i;
	return -1;
}

int
timerfd_settime(int fd, int flags, const struct itimerspec *new,
	struct itimerspec *old)
{
	int i;
	uint64_t now, first;
	sigset_t omask;

	if ((i = tfd_find(fd)) < 0) { errno = EBADF; return -1; }

	if (old) {
		uint64_t remain = tfds[i].next_ns == UINT64_MAX ? 0 :
		    (now = now_ns(CLOCK_REALTIME),
		     tfds[i].next_ns > now ? tfds[i].next_ns - now : 0);
		memset(old, 0, sizeof(*old));
		old->it_value.tv_sec = remain / 1000000000ULL;
		old->it_value.tv_nsec = remain % 1000000000ULL;
		old->it_interval.tv_sec = tfds[i].period_ns / 1000000000ULL;
		old->it_interval.tv_nsec = tfds[i].period_ns % 1000000000ULL;
	}

	if ((flags & ~(TFD_TIMER_ABSTIME | TFD_TIMER_CANCEL_SET)) != 0) {
		errno = EINVAL;
		return -1;
	}

	tfd_lock(&omask);
	if (new->it_value.tv_sec == 0 && new->it_value.tv_nsec == 0) {
		tfds[i].next_ns = UINT64_MAX;	/* disarm */
	} else {
		first = (uint64_t)new->it_value.tv_sec * 1000000000ULL +
		    new->it_value.tv_nsec;
		if (flags & TFD_TIMER_ABSTIME) {
			tfds[i].next_ns = first;
		} else {
			tfds[i].next_ns = now_ns(CLOCK_REALTIME) + first;
		}
	}
	tfds[i].period_ns =
	    (uint64_t)new->it_interval.tv_sec * 1000000000ULL +
	    new->it_interval.tv_nsec;

	tfd_rearm_itimer();
	tfd_unlock(&omask);
	return 0;
}

int
timerfd_gettime(int fd, struct itimerspec *cur)
{
	int i;
	uint64_t now, remain;

	if ((i = tfd_find(fd)) < 0) { errno = EBADF; return -1; }
	remain = 0;
	if (tfds[i].next_ns != UINT64_MAX) {
		now = now_ns(CLOCK_REALTIME);
		remain = tfds[i].next_ns > now ? tfds[i].next_ns - now : 0;
	}
	memset(cur, 0, sizeof(*cur));
	cur->it_value.tv_sec = remain / 1000000000ULL;
	cur->it_value.tv_nsec = remain % 1000000000ULL;
	cur->it_interval.tv_sec = tfds[i].period_ns / 1000000000ULL;
	cur->it_interval.tv_nsec = tfds[i].period_ns % 1000000000ULL;
	return 0;
}
