/* test98: Linux-compatible epoll (libc emulation over poll(2)).
 *
 * Covers what wayland-server's event loop and similar users rely on:
 * registration errors, level-triggered readiness with user data, MOD/DEL,
 * EPOLLONESHOT, hangup, timeouts, EINTR, closed members dropping out,
 * eventfd/timerfd/signalfd members, maxevents fairness, fork inheritance,
 * CLOEXEC and reuse of a closed epoll fd number.
 */

#include <sys/types.h>
#include <sys/epoll.h>
#include <sys/eventfd.h>
#include <sys/signalfd.h>
#include <sys/socket.h>
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
#include <unistd.h>

int max_error = 5;
#include "common.h"

static long
ms_since(struct timeval *t0)
{
	struct timeval t1;

	gettimeofday(&t1, NULL);
	return (t1.tv_sec - t0->tv_sec) * 1000 + (t1.tv_usec - t0->tv_usec) / 1000;
}

static int
add(int ep, int fd, uint32_t ev, uint64_t data)
{
	struct epoll_event e;

	memset(&e, 0, sizeof(e));
	e.events = ev;
	e.data.u64 = data;
	return epoll_ctl(ep, EPOLL_CTL_ADD, fd, &e);
}

static void
test_basic(void)
{
	struct epoll_event e, out[4];
	int ep, p[2], q[2], fd;

	subtest = 1;
	if (epoll_create(0) != -1 || errno != EINVAL) e(1);
	if (epoll_create1(12345) != -1 || errno != EINVAL) e(2);
	if ((ep = epoll_create1(EPOLL_CLOEXEC)) < 0) { e(3); return; }
	if (!(fcntl(ep, F_GETFD) & FD_CLOEXEC)) e(4);
	if (pipe(p) != 0 || pipe(q) != 0) e(5);

	/* Errors. */
	if (add(ep, 9999, EPOLLIN, 0) != -1 || errno != EBADF) e(6);
	if (add(p[0], q[0], EPOLLIN, 0) != -1 || errno != EINVAL) e(7);
	if (add(ep, ep, EPOLLIN, 0) != -1 || errno != EINVAL) e(8);
	if ((fd = open("f98", O_RDWR | O_CREAT, 0644)) < 0) e(9);
	if (add(ep, fd, EPOLLIN, 0) != -1 || errno != EPERM) e(10);
	close(fd);
	if (add(ep, p[0], EPOLLIN | EPOLLET, 0) != -1 || errno != EINVAL)
		e(11);
	if (add(ep, p[0], EPOLLIN, 0x1111) != 0) e(12);
	if (add(ep, p[0], EPOLLIN, 0x1111) != -1 || errno != EEXIST) e(13);
	if (epoll_ctl(ep, EPOLL_CTL_MOD, q[0], &e) != -1 || errno != ENOENT)
		e(14);
	if (epoll_ctl(ep, EPOLL_CTL_DEL, q[0], NULL) != -1 || errno != ENOENT)
		e(15);
	if (epoll_ctl(ep, 99, p[0], &e) != -1 || errno != EINVAL) e(16);
	if (epoll_wait(ep, out, 0, 0) != -1 || errno != EINVAL) e(17);

	/* Level-triggered readiness with user data. */
	if (epoll_wait(ep, out, 4, 0) != 0) e(18);
	if (write(p[1], "x", 1) != 1) e(19);
	if (epoll_wait(ep, out, 4, 0) != 1) e(20);
	if (out[0].events != EPOLLIN || out[0].data.u64 != 0x1111) e(21);
	if (epoll_wait(ep, out, 4, 0) != 1) e(22);	/* still readable */

	/* MOD: now interested in output of the write end too. */
	if (add(ep, q[1], EPOLLOUT, 0x2222) != 0) e(23);
	if (epoll_wait(ep, out, 4, 0) != 2) e(24);
	memset(&e, 0, sizeof(e));
	e.events = EPOLLPRI;		/* never ready on a pipe */
	e.data.ptr = &e;
	if (epoll_ctl(ep, EPOLL_CTL_MOD, p[0], &e) != 0) e(25);
	if (epoll_wait(ep, out, 4, 0) != 1 || out[0].data.u64 != 0x2222)
		e(26);

	/* DEL. */
	if (epoll_ctl(ep, EPOLL_CTL_DEL, q[1], NULL) != 0) e(27);
	if (epoll_wait(ep, out, 4, 0) != 0) e(28);

	/* Hangup: the writer goes away. */
	e.events = EPOLLIN;
	e.data.fd = p[0];
	if (epoll_ctl(ep, EPOLL_CTL_MOD, p[0], &e) != 0) e(29);
	{
		char c;
		if (read(p[0], &c, 1) != 1) e(30);
	}
	close(p[1]);
	if (epoll_wait(ep, out, 4, 100) != 1) e(31);
	/* Linux reports EPOLLHUP; MINIX poll() only knows "readable" (EOF). */
	if (!(out[0].events & (EPOLLHUP | EPOLLIN)) || out[0].data.fd != p[0])
		e(32);

	/* A member closed without DEL drops out silently. */
	close(p[0]);
	if (epoll_wait(ep, out, 4, 0) != 0) e(33);
	if (add(ep, q[0], EPOLLIN, 7) != 0) e(34);
	close(q[0]);
	close(q[1]);
	if (epoll_wait(ep, out, 4, 0) != 0) e(35);
	close(ep);

	/* The closed epoll fd number is no longer an epoll fd, even when
	 * reused by something else. */
	if (pipe(p) != 0) e(36);
	if (add(ep, p[1], EPOLLOUT, 0) != -1) e(37);
	close(p[0]);
	close(p[1]);
}

static void
test_oneshot_timeout(void)
{
	struct epoll_event out[2];
	struct timeval t0;
	int ep, p[2];
	long ms;

	subtest = 2;
	if ((ep = epoll_create1(0)) < 0 || pipe(p) != 0) { e(1); return; }
	if (add(ep, p[0], EPOLLIN | EPOLLONESHOT, 5) != 0) e(2);
	if (write(p[1], "ab", 2) != 2) e(3);
	if (epoll_wait(ep, out, 2, 0) != 1 || out[0].data.u64 != 5) e(4);
	if (epoll_wait(ep, out, 2, 0) != 0) e(5);	/* disarmed */
	{
		struct epoll_event e;
		memset(&e, 0, sizeof(e));
		e.events = EPOLLIN | EPOLLONESHOT;
		e.data.u64 = 6;
		if (epoll_ctl(ep, EPOLL_CTL_MOD, p[0], &e) != 0) e(6);
	}
	if (epoll_wait(ep, out, 2, 0) != 1 || out[0].data.u64 != 6) e(7);

	/* Timeout. */
	{
		char buf[4];
		if (read(p[0], buf, sizeof(buf)) != 2) e(8);
		struct epoll_event e;
		memset(&e, 0, sizeof(e));
		e.events = EPOLLIN;
		if (epoll_ctl(ep, EPOLL_CTL_MOD, p[0], &e) != 0) e(9);
	}
	gettimeofday(&t0, NULL);
	if (epoll_wait(ep, out, 2, 300) != 0) e(10);
	ms = ms_since(&t0);
	if (ms < 250 || ms > 1500) e(11);

	/* Blocking wait woken by a child writing. */
	if (fork() == 0) {
		usleep(200000);
		(void)write(p[1], "z", 1);
		_exit(0);
	}
	gettimeofday(&t0, NULL);
	if (epoll_wait(ep, out, 2, -1) != 1) e(12);
	ms = ms_since(&t0);
	if (ms < 100) e(13);
	wait(NULL);
	close(p[0]);
	close(p[1]);
	close(ep);
}

static void
on_alarm(int sig)
{
	(void)sig;
}

static void
test_eintr(void)
{
	struct epoll_event out[1];
	struct sigaction sa;
	int ep, p[2];

	subtest = 3;
	if ((ep = epoll_create1(0)) < 0 || pipe(p) != 0) { e(1); return; }
	if (add(ep, p[0], EPOLLIN, 0) != 0) e(2);
	memset(&sa, 0, sizeof(sa));
	sa.sa_handler = on_alarm;
	sigaction(SIGALRM, &sa, NULL);		/* no SA_RESTART */
	alarm(1);
	if (epoll_wait(ep, out, 1, 5000) != -1 || errno != EINTR) e(3);
	signal(SIGALRM, SIG_DFL);
	close(p[0]);
	close(p[1]);
	close(ep);
}

static void
test_lfd_members(void)
{
	struct epoll_event out[4];
	struct itimerspec its;
	sigset_t mask;
	int ep, efd, tfd, sfd, seen, n, i;

	subtest = 4;
	if ((ep = epoll_create1(0)) < 0) { e(1); return; }
	if ((efd = eventfd(0, EFD_NONBLOCK)) < 0) e(2);
	if ((tfd = timerfd_create(CLOCK_MONOTONIC, TFD_NONBLOCK)) < 0) e(3);
	sigemptyset(&mask);
	sigaddset(&mask, SIGUSR1);
	sigprocmask(SIG_BLOCK, &mask, NULL);	/* the usual signalfd idiom */
	if ((sfd = signalfd(-1, &mask, SFD_NONBLOCK)) < 0) e(4);
	{
		sigset_t cur;
		sigprocmask(SIG_BLOCK, NULL, &cur);
		if (sigismember(&cur, SIGUSR1) != 1) e(41);	/* still "blocked" */
	}
	if (add(ep, efd, EPOLLIN, 1) != 0) e(5);
	if (add(ep, tfd, EPOLLIN, 2) != 0) e(6);
	if (add(ep, sfd, EPOLLIN, 4) != 0) e(7);

	if (epoll_wait(ep, out, 4, 0) != 0) e(8);
	eventfd_write(efd, 3);
	memset(&its, 0, sizeof(its));
	its.it_value.tv_nsec = 50 * 1000 * 1000;
	if (timerfd_settime(tfd, 0, &its, NULL) != 0) e(9);
	kill(getpid(), SIGUSR1);

	/* All three become ready within two seconds.  Level-triggered: once
	 * a member has reported, take it out so the wait can block again. */
	for (seen = 0, i = 0; i < 20 && seen != 7; i++) {
		n = epoll_wait(ep, out, 4, 100);
		while (n-- > 0) {
			int bit = (int)out[n].data.u64;
			seen |= bit;
			if (epoll_ctl(ep, EPOLL_CTL_DEL, bit == 1 ? efd :
			    bit == 2 ? tfd : sfd, NULL) != 0) e(11);
		}
	}
	if (seen != 7) e(10);

	close(efd);
	close(tfd);
	close(sfd);
	sigprocmask(SIG_UNBLOCK, &mask, NULL);
	close(ep);
}

static void
test_socket_hup(void)
{
	struct epoll_event out[2];
	int ep, sv[2];

	subtest = 6;
	if ((ep = epoll_create1(0)) < 0) { e(1); return; }
	if (socketpair(AF_UNIX, SOCK_STREAM, 0, sv) != 0) { e(2); return; }
	if (add(ep, sv[0], EPOLLIN | EPOLLRDHUP, 9) != 0) e(3);
	if (write(sv[1], "x", 1) != 1) e(4);
	if (epoll_wait(ep, out, 2, 0) != 1 || out[0].events != EPOLLIN) e(5);
	close(sv[1]);		/* data still pending: no HUP yet ... */
	if (epoll_wait(ep, out, 2, 0) != 1 || (out[0].events & EPOLLHUP)) e(6);
	{
		char c;
		if (read(sv[0], &c, 1) != 1) e(7);
	}
	/* ... then the peer's close is a hangup, as on Linux. */
	if (epoll_wait(ep, out, 2, 0) != 1) e(8);
	if ((out[0].events & (EPOLLHUP | EPOLLRDHUP)) !=
	    (EPOLLHUP | EPOLLRDHUP)) e(9);
	close(sv[0]);
	close(ep);
}

static void
test_fairness_fork(void)
{
	struct epoll_event out[2];
	int ep, p[4][2], i, n, seen = 0, status;
	pid_t pid;

	subtest = 5;
	if ((ep = epoll_create1(0)) < 0) { e(1); return; }
	for (i = 0; i < 4; i++) {
		if (pipe(p[i]) != 0) e(2);
		if (add(ep, p[i][0], EPOLLIN, 1 << i) != 0) e(3);
		if (write(p[i][1], "x", 1) != 1) e(4);
	}
	/* With maxevents 2, all four ready fds get reported over 2 calls. */
	for (i = 0; i < 2; i++) {
		n = epoll_wait(ep, out, 2, 0);
		if (n != 2) e(5);
		while (n-- > 0)
			seen |= (int)out[n].data.u64;
	}
	if (seen != 15) e(6);

	/* A fork child inherits a working copy of the interest list. */
	if ((pid = fork()) == 0) {
		int c = epoll_wait(ep, out, 2, 0);
		_exit(c == 2 ? 0 : 1);
	}
	if (pid < 0 || waitpid(pid, &status, 0) != pid ||
	    !WIFEXITED(status) || WEXITSTATUS(status) != 0) e(7);

	for (i = 0; i < 4; i++) {
		close(p[i][0]);
		close(p[i][1]);
	}
	close(ep);
}

/*
 * An epoll fd can be polled and nested into another epoll instance: it is
 * readable while one of its members is (libinput's fd inside a Wayland
 * event loop).
 */
static void
test_nested(void)
{
	struct epoll_event ev, out[4];
	struct pollfd pfd;
	int inner, outer, p[2], q[2];
	char c;

	subtest = 7;

	if (pipe(p) != 0 || pipe(q) != 0) e(1);
	if ((inner = epoll_create1(EPOLL_CLOEXEC)) < 0) e(2);
	if ((outer = epoll_create1(EPOLL_CLOEXEC)) < 0) e(3);
	ev.events = EPOLLIN;
	ev.data.u32 = 1;
	if (epoll_ctl(inner, EPOLL_CTL_ADD, p[0], &ev) != 0) e(4);
	ev.data.u32 = 2;
	if (epoll_ctl(outer, EPOLL_CTL_ADD, inner, &ev) != 0) e(5);
	ev.data.u32 = 3;
	if (epoll_ctl(outer, EPOLL_CTL_ADD, q[0], &ev) != 0) e(6);

	/* Nothing ready: poll() and epoll_wait() time out. */
	pfd.fd = inner;
	pfd.events = POLLIN;
	if (poll(&pfd, 1, 50) != 0) e(7);
	if (epoll_wait(outer, out, 4, 50) != 0) e(8);

	/* A member of the inner instance becomes ready. */
	if (write(p[1], "x", 1) != 1) e(9);
	if (poll(&pfd, 1, 1000) != 1 || !(pfd.revents & POLLIN)) e(10);
	if (epoll_wait(outer, out, 4, 1000) != 1) e(11);
	if (out[0].data.u32 != 2 || !(out[0].events & EPOLLIN)) e(12);
	if (epoll_wait(inner, out, 4, 0) != 1 || out[0].data.u32 != 1) e(13);

	/* Drained: quiet again; a direct member of outer still works. */
	if (read(p[0], &c, 1) != 1) e(14);
	if (poll(&pfd, 1, 0) != 0) e(15);
	if (write(q[1], "y", 1) != 1) e(16);
	if (epoll_wait(outer, out, 4, 1000) != 1 || out[0].data.u32 != 3)
		e(17);
	if (read(q[0], &c, 1) != 1) e(18);

	/* Blocking: a writer in a child wakes a poll() on the outer fd. */
	switch (fork()) {
	case -1: e(19); break;
	case 0:
		usleep(200000);
		write(p[1], "z", 1);
		_exit(0);
	default:
		pfd.fd = outer;
		if (poll(&pfd, 1, 5000) != 1 || !(pfd.revents & POLLIN))
			e(20);
		wait(NULL);
	}

	/* A closed member of the inner instance is dropped, not EBADF. */
	if (read(p[0], &c, 1) != 1) e(21);
	close(p[0]);
	pfd.fd = inner;
	if (poll(&pfd, 1, 0) != 0) e(22);

	close(p[1]);
	close(q[0]);
	close(q[1]);
	close(outer);
	close(inner);
}

int
main(int argc, char **argv)
{
	start(98);

	test_basic();
	test_oneshot_timeout();
	test_eintr();
	test_lfd_members();
	test_socket_hup();
	test_fairness_fork();
	test_nested();

	quit();
	return 0;
}
