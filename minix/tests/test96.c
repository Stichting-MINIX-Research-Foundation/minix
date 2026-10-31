/* test96: Linux-compatible eventfd, timerfd and signalfd libc shims.
 *
 * These are implemented in MINIX libc over socketpair/pipe plumbing plus
 * setitimer/signal handlers (see minix/lib/libc/sys/{event,timer,signal}fd.c).
 * The test checks the behavior ported software (wayland, wlroots) relies on:
 * counter semantics, nonblocking reads, poll(2) readiness and that blocking
 * calls keep working while the libc SIGALRM/signalfd handlers fire.
 */

#include <sys/types.h>
#include <sys/eventfd.h>
#include <sys/signalfd.h>
#include <sys/time.h>
#include <sys/timerfd.h>
#include <sys/wait.h>
#include <errno.h>
#include <fcntl.h>
#include <poll.h>
#include <signal.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <unistd.h>

int max_error = 5;
#include "common.h"

static int
readable(int fd, int timeout_ms)
{
	struct pollfd pfd;
	int r;

	pfd.fd = fd;
	pfd.events = POLLIN;
	pfd.revents = 0;
	r = poll(&pfd, 1, timeout_ms);
	if (r < 0) return -1;
	return r == 1 && (pfd.revents & POLLIN);
}

static uint64_t
ms_now(void)
{
	struct timespec ts;

	clock_gettime(CLOCK_MONOTONIC, &ts);
	return (uint64_t)ts.tv_sec * 1000 + ts.tv_nsec / 1000000;
}

/* 1. eventfd counter semantics. */
static void
test_eventfd(void)
{
	eventfd_t v;
	int fd;

	subtest = 1;
	if ((fd = eventfd(0, EFD_NONBLOCK)) < 0) { e(1); return; }

	if (readable(fd, 0) != 0) e(2);
	if (eventfd_read(fd, &v) == 0) e(3);
	else if (errno != EAGAIN && errno != EWOULDBLOCK) e(4);

	if (eventfd_write(fd, 1) != 0) e(5);
	if (eventfd_write(fd, 4) != 0) e(6);
	if (readable(fd, 1000) != 1) e(7);
	if (eventfd_read(fd, &v) != 0 || v != 5) e(8);	/* drain and sum */
	if (readable(fd, 0) != 0) e(9);
	close(fd);

	/* initval */
	if ((fd = eventfd(3, EFD_NONBLOCK)) < 0) { e(10); return; }
	if (eventfd_read(fd, &v) != 0 || v != 3) e(11);
	close(fd);

	/* EFD_SEMAPHORE: one unit per read */
	if ((fd = eventfd(2, EFD_NONBLOCK | EFD_SEMAPHORE)) < 0) {
		e(12);
		return;
	}
	if (eventfd_read(fd, &v) != 0 || v != 1) e(13);
	if (eventfd_read(fd, &v) != 0 || v != 1) e(14);
	if (eventfd_read(fd, &v) == 0) e(15);
	close(fd);

	/* EFD_CLOEXEC */
	if ((fd = eventfd(0, EFD_CLOEXEC)) < 0) { e(16); return; }
	if (!(fcntl(fd, F_GETFD) & FD_CLOEXEC)) e(17);
	/* blocking read returns once the counter is nonzero */
	if (eventfd_write(fd, 2) != 0) e(18);
	if (eventfd_read(fd, &v) != 0 || v != 2) e(19);
	close(fd);

	/* a recycled fd number must not inherit stale state */
	if ((fd = eventfd(0, EFD_NONBLOCK)) < 0) { e(20); return; }
	if (eventfd_write(fd, 1) != 0) e(21);
	if (eventfd_read(fd, &v) != 0 || v != 1) e(22);
	close(fd);

	if (eventfd_write(-1, 1) == 0 || errno != EBADF) e(23);
}

/* 2. timerfd one-shot, periodic, gettime, disarm. */
static void
test_timerfd(void)
{
	struct itimerspec its, cur;
	uint64_t cnt, t0, dt;
	int fd;

	subtest = 2;
	if ((fd = timerfd_create(CLOCK_MONOTONIC, TFD_NONBLOCK)) < 0) {
		e(30);
		return;
	}
	if (read(fd, &cnt, 8) != -1 ||
	    (errno != EAGAIN && errno != EWOULDBLOCK)) e(31);

	/* disarmed timer reports zero */
	if (timerfd_gettime(fd, &cur) != 0) e(32);
	if (cur.it_value.tv_sec != 0 || cur.it_value.tv_nsec != 0) e(33);

	/* one-shot 200ms */
	memset(&its, 0, sizeof(its));
	its.it_value.tv_nsec = 200 * 1000000;
	t0 = ms_now();
	if (timerfd_settime(fd, 0, &its, NULL) != 0) e(34);
	if (timerfd_gettime(fd, &cur) != 0) e(35);
	if (cur.it_value.tv_sec != 0 || cur.it_value.tv_nsec == 0 ||
	    cur.it_value.tv_nsec > 200 * 1000000) e(36);
	if (readable(fd, 5000) != 1) e(37);
	dt = ms_now() - t0;
	if (dt < 150) e(38);	/* fired too early (allow clock granularity) */
	if (read(fd, &cnt, 8) != 8 || cnt != 1) e(39);
	if (readable(fd, 400) != 0) e(40);	/* one-shot: no refire */

	/* periodic 50ms: several expirations accumulate over ~1s */
	its.it_value.tv_nsec = 50 * 1000000;
	its.it_interval.tv_nsec = 50 * 1000000;
	if (timerfd_settime(fd, 0, &its, NULL) != 0) e(41);
	t0 = ms_now();
	cnt = 0;
	while (ms_now() - t0 < 1000) {
		uint64_t c;
		if (readable(fd, 500) == 1 && read(fd, &c, 8) == 8)
			cnt += c;
	}
	if (cnt < 5) e(42);

	/* usleep must not be starved by the periodic SIGALRM traffic */
	t0 = ms_now();
	if (usleep(300000) != 0 && errno != EINTR) e(43);
	if (ms_now() - t0 > 3000) e(44);

	/* the old value is reported, then the timer is disarmed */
	memset(&its, 0, sizeof(its));
	if (timerfd_settime(fd, 0, &its, &cur) != 0) e(45);
	if (cur.it_interval.tv_nsec != 50 * 1000000) e(46);
	while (read(fd, &cnt, 8) == 8)
		;	/* drain */
	if (readable(fd, 300) != 0) e(47);

	if (timerfd_settime(fd, 0x80, &its, NULL) == 0 || errno != EINVAL)
		e(48);
	close(fd);

	if (timerfd_gettime(-1, &cur) == 0 || errno != EBADF) e(49);
}

/* 3. signalfd: delivery as a record, poll readiness, mask update. */
static void
test_signalfd(void)
{
	struct signalfd_siginfo ssi;
	sigset_t mask;
	pid_t pid;
	int fd;

	subtest = 3;
	if (sizeof(struct signalfd_siginfo) != 128) e(60);

	sigemptyset(&mask);
	sigaddset(&mask, SIGUSR1);
	if ((fd = signalfd(-1, &mask, SFD_NONBLOCK)) < 0) { e(61); return; }
	if (read(fd, &ssi, sizeof(ssi)) != -1 ||
	    (errno != EAGAIN && errno != EWOULDBLOCK)) e(62);

	/* a signal from ourselves becomes a record (and doesn't kill us) */
	if (kill(getpid(), SIGUSR1) != 0) e(63);
	if (readable(fd, 2000) != 1) e(64);
	if (read(fd, &ssi, sizeof(ssi)) != sizeof(ssi)) e(65);
	else if (ssi.ssi_signo != SIGUSR1) e(66);

	/* from another process */
	switch ((pid = fork())) {
	case -1:
		e(67);
		break;
	case 0:
		exit(kill(getppid(), SIGUSR1) != 0);
	default:
		if (readable(fd, 5000) != 1) e(68);
		if (read(fd, &ssi, sizeof(ssi)) != sizeof(ssi) ||
		    ssi.ssi_signo != SIGUSR1) e(69);
		(void)waitpid(pid, NULL, 0);
	}

	/* mask update adds SIGUSR2 */
	sigaddset(&mask, SIGUSR2);
	if (signalfd(fd, &mask, 0) != fd) e(70);
	if (kill(getpid(), SIGUSR2) != 0) e(71);
	if (readable(fd, 2000) != 1) e(72);
	if (read(fd, &ssi, sizeof(ssi)) != sizeof(ssi) ||
	    ssi.ssi_signo != SIGUSR2) e(73);

	/* SIGKILL can't be caught; bad fd / NULL mask are rejected */
	sigemptyset(&mask);
	sigaddset(&mask, SIGKILL);
	if (signalfd(-1, &mask, 0) >= 0 || errno != EINVAL) e(74);
	if (signalfd(-1, NULL, 0) >= 0 || errno != EINVAL) e(75);
	sigemptyset(&mask);
	if (signalfd(12345, &mask, 0) >= 0 || errno != EBADF) e(76);

	close(fd);
	signal(SIGUSR1, SIG_DFL);
	signal(SIGUSR2, SIG_DFL);
}

/* 4. All three multiplexed in one poll loop (the wayland event loop). */
static void
test_mixed_poll(void)
{
	struct pollfd pfd[3];
	struct itimerspec its;
	sigset_t mask;
	eventfd_t v;
	uint64_t t0;
	int efd, tfd, sfd, seen = 0;

	subtest = 4;
	efd = eventfd(0, EFD_NONBLOCK);
	tfd = timerfd_create(CLOCK_MONOTONIC, TFD_NONBLOCK);
	sigemptyset(&mask);
	sigaddset(&mask, SIGUSR1);
	sfd = signalfd(-1, &mask, SFD_NONBLOCK);
	if (efd < 0 || tfd < 0 || sfd < 0) { e(80); return; }

	memset(&its, 0, sizeof(its));
	its.it_value.tv_nsec = 100 * 1000000;
	if (timerfd_settime(tfd, 0, &its, NULL) != 0) e(81);
	if (eventfd_write(efd, 1) != 0) e(82);
	if (kill(getpid(), SIGUSR1) != 0) e(83);

	t0 = ms_now();
	while (seen != 7 && ms_now() - t0 < 5000) {
		pfd[0].fd = efd; pfd[1].fd = tfd; pfd[2].fd = sfd;
		pfd[0].events = pfd[1].events = pfd[2].events = POLLIN;
		pfd[0].revents = pfd[1].revents = pfd[2].revents = 0;
		if (poll(pfd, 3, 1000) < 0) {
			if (errno == EINTR) continue;
			e(84);
			break;
		}
		if (pfd[0].revents & POLLIN) {
			if (eventfd_read(efd, &v) == 0) seen |= 1;
		}
		if (pfd[1].revents & POLLIN) {
			uint64_t c;
			if (read(tfd, &c, 8) == 8) seen |= 2;
		}
		if (pfd[2].revents & POLLIN) {
			struct signalfd_siginfo ssi;
			if (read(sfd, &ssi, sizeof(ssi)) == sizeof(ssi))
				seen |= 4;
		}
	}
	if (seen != 7) e(85);

	close(efd);
	close(tfd);
	close(sfd);
	signal(SIGUSR1, SIG_DFL);
}

/* 5. Closed instances must not raise SIGPIPE, the shims must not change the
 * process signal mask, and fork must not pass pending signals on (PM). */
static void
test_no_sigpipe(void)
{
	struct itimerspec its;
	sigset_t mask, pend, omask;
	pid_t pid;
	int fd, status;

	subtest = 5;

	/* signalfd must not block SIGPIPE (the mask is inherited by exec) */
	sigemptyset(&mask);
	sigaddset(&mask, SIGUSR1);
	if ((fd = signalfd(-1, &mask, SFD_NONBLOCK)) < 0) { e(90); return; }
	if (sigprocmask(SIG_BLOCK, NULL, &omask) != 0) e(91);
	if (sigismember(&omask, SIGPIPE)) e(92);

	/* the handler of a closed signalfd writes into a reader-less pipe:
	 * that must neither kill us nor leave a SIGPIPE pending */
	close(fd);
	if (kill(getpid(), SIGUSR1) != 0) e(93);
	if (sigpending(&pend) != 0 || sigismember(&pend, SIGPIPE)) e(94);
	signal(SIGUSR1, SIG_DFL);

	/* same for an armed timerfd that gets closed */
	if ((fd = timerfd_create(CLOCK_MONOTONIC, 0)) < 0) { e(95); return; }
	memset(&its, 0, sizeof(its));
	its.it_value.tv_nsec = 50 * 1000000;
	if (timerfd_settime(fd, 0, &its, NULL) != 0) e(96);
	close(fd);
	usleep(200000);
	if (sigpending(&pend) != 0 || sigismember(&pend, SIGPIPE)) e(97);

	/* and eventfd_write on a closed eventfd reports EIO, no signal */
	if ((fd = eventfd(0, 0)) < 0) { e(98); return; }
	close(fd);
	if (eventfd_write(fd, 1) == 0) e(99);
	else if (errno != EIO && errno != EBADF) e(100);
	if (sigpending(&pend) != 0 || sigismember(&pend, SIGPIPE)) e(101);

	/* POSIX: a forked child starts with no pending signals */
	sigemptyset(&mask);
	sigaddset(&mask, SIGUSR2);
	if (sigprocmask(SIG_BLOCK, &mask, &omask) != 0) e(102);
	if (kill(getpid(), SIGUSR2) != 0) e(103);
	if (sigpending(&pend) != 0 || !sigismember(&pend, SIGUSR2)) e(104);
	switch ((pid = fork())) {
	case -1:
		e(105);
		break;
	case 0:
		exit(sigpending(&pend) != 0 || sigismember(&pend, SIGUSR2));
	default:
		if (waitpid(pid, &status, 0) != pid || !WIFEXITED(status) ||
		    WEXITSTATUS(status) != 0) e(106);
	}
	/* discard our own pending SIGUSR2, then restore */
	signal(SIGUSR2, SIG_IGN);
	sigprocmask(SIG_SETMASK, &omask, NULL);
	signal(SIGUSR2, SIG_DFL);
}

int
main(int argc, char **argv)
{
	start(96);

	test_eventfd();
	test_timerfd();
	test_signalfd();
	test_mixed_poll();
	test_no_sigpipe();

	quit();
	return 0;
}
