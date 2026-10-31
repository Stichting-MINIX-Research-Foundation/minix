/* Tests for how select(2) and poll(2) answer for descriptors that are not
 * open.
 *
 * The two differ on purpose.  select(2) fails the whole call with EBADF,
 * whatever else was in its sets; poll(2) answers that one entry with
 * POLLNVAL and goes on with the rest, which is what lets a program poll a
 * set that it does not fully control.  An fd_set is FD_SETSIZE wide while a
 * process can only have OPEN_MAX descriptors, so the bits in between are
 * bad descriptors too, not a bad argument.
 *
 * The last subtest is about the other end of the same question: when a pipe
 * is writable.  It is not whenever a byte would fit, but when a whole
 * atomic write would -- and something has to wake a waiter once that
 * becomes true.
 *
 * An alarm guards every wait: a regression here is as likely to hang as to
 * answer wrongly.
 */
#include <sys/select.h>
#include <sys/wait.h>
#include <fcntl.h>
#include <limits.h>
#include <poll.h>
#include <signal.h>

#include "common.h"

static int pipefd[2] = { -1, -1 };

static void
alarm_handler(int sig)
{

	/* Nothing: the point is to interrupt a wait that should not have
	 * happened.  A blocked call returns EINTR and the check fails. */
	(void)sig;
}

static void
guard(unsigned int secs)
{
	struct sigaction sa;

	memset(&sa, 0, sizeof(sa));
	sa.sa_handler = alarm_handler;
	sigemptyset(&sa.sa_mask);
	if (sigaction(SIGALRM, &sa, NULL) != 0) e(0);
	alarm(secs);
}

/*
 * Find a descriptor that is not open.
 */
static int
closed_fd(void)
{
	int fd;

	for (fd = 3; fd < OPEN_MAX; fd++)
		if (fcntl(fd, F_GETFD) == -1 && errno == EBADF)
			return fd;

	e(0);
	return -1;
}

/*
 * Is the given descriptor writable right now?  Returns 1, 0, or -1.
 */
static int
writable(int fd, int secs)
{
	struct timeval tv;
	fd_set wr;

	FD_ZERO(&wr);
	FD_SET(fd, &wr);
	tv.tv_sec = secs;
	tv.tv_usec = 0;

	guard(10);
	switch (select(fd + 1, NULL, &wr, NULL, &tv)) {
	case -1:
		alarm(0);
		return -1;
	case 0:
		alarm(0);
		return 0;
	default:
		alarm(0);
		return FD_ISSET(fd, &wr) ? 1 : -1;
	}
}

/*
 * select(2) rejects a descriptor that is not open, wherever in the sets it
 * sits.
 */
static void
test99a(void)
{
	fd_set rd;
	int bad;

	subtest = 1;

	bad = closed_fd();

	FD_ZERO(&rd);
	FD_SET(bad, &rd);
	guard(5);
	errno = 0;
	if (select(bad + 1, &rd, NULL, NULL, NULL) != -1) e(0);
	if (errno != EBADF) e(0);
	alarm(0);

	/* A descriptor beyond OPEN_MAX cannot be open either: that is a bad
	 * descriptor, not a bad nfds. */
	FD_ZERO(&rd);
	FD_SET(OPEN_MAX, &rd);
	guard(5);
	errno = 0;
	if (select(OPEN_MAX + 1, &rd, NULL, NULL, NULL) != -1) e(0);
	if (errno != EBADF) e(0);
	alarm(0);

	FD_ZERO(&rd);
	FD_SET(FD_SETSIZE - 1, &rd);
	guard(5);
	errno = 0;
	if (select(FD_SETSIZE, &rd, NULL, NULL, NULL) != -1) e(0);
	if (errno != EBADF) e(0);
	alarm(0);

	/* The write and exception sets are no different. */
	FD_ZERO(&rd);
	FD_SET(bad, &rd);
	errno = 0;
	if (select(bad + 1, NULL, &rd, NULL, NULL) != -1) e(0);
	if (errno != EBADF) e(0);

	errno = 0;
	if (select(bad + 1, NULL, NULL, &rd, NULL) != -1) e(0);
	if (errno != EBADF) e(0);
}

/*
 * A set that is FD_SETSIZE wide but has nothing set above OPEN_MAX is a
 * perfectly ordinary call: the excess is ignored.  Only a range that cannot
 * be expressed at all is EINVAL.
 */
static void
test99b(void)
{
	struct timeval tv;
	fd_set rd;

	subtest = 2;

	if (write(pipefd[1], "x", 1) != 1) e(0);

	FD_ZERO(&rd);
	FD_SET(pipefd[0], &rd);
	tv.tv_sec = 5;
	tv.tv_usec = 0;
	guard(10);
	if (select(FD_SETSIZE, &rd, NULL, NULL, &tv) != 1) e(0);
	if (!FD_ISSET(pipefd[0], &rd)) e(0);
	alarm(0);

	FD_ZERO(&rd);
	errno = 0;
	if (select(FD_SETSIZE + 1, &rd, NULL, NULL, &tv) != -1) e(0);
	if (errno != EINVAL) e(0);

	errno = 0;
	if (select(-1, &rd, NULL, NULL, &tv) != -1) e(0);
	if (errno != EINVAL) e(0);

	/* Drain the pipe again. */
	{
		char c;

		if (read(pipefd[0], &c, 1) != 1) e(0);
	}
}

/*
 * poll(2) answers a bad descriptor with POLLNVAL, and keeps its promises
 * about the others in the same call.
 */
static void
test99c(void)
{
	struct pollfd pfd[4];
	int bad;

	subtest = 3;

	bad = closed_fd();

	/* One bad descriptor on its own: answered at once, even though the
	 * call was told to wait forever. */
	memset(pfd, 0, sizeof(pfd));
	pfd[0].fd = bad;
	pfd[0].events = POLLIN;
	guard(5);
	if (poll(pfd, 1, -1) != 1) e(0);
	if (pfd[0].revents != POLLNVAL) e(0);
	alarm(0);

	/* A bad descriptor next to a good one: both are answered.  This is
	 * the shape that matters -- a program polling a set it did not build
	 * itself must not lose the whole call to one closed descriptor. */
	if (write(pipefd[1], "x", 1) != 1) e(0);

	memset(pfd, 0, sizeof(pfd));
	pfd[0].fd = bad;
	pfd[0].events = POLLIN;
	pfd[1].fd = pipefd[0];
	pfd[1].events = POLLIN;
	pfd[2].fd = -1;			/* ignored entirely */
	pfd[2].events = POLLIN;
	pfd[3].fd = FD_SETSIZE;		/* too wide to ever be open */
	pfd[3].events = POLLIN;

	guard(5);
	if (poll(pfd, 4, -1) != 3) e(0);
	alarm(0);
	if (pfd[0].revents != POLLNVAL) e(0);
	if (!(pfd[1].revents & POLLIN)) e(0);
	if (pfd[2].revents != 0) e(0);
	if (pfd[3].revents != POLLNVAL) e(0);

	{
		char c;

		if (read(pipefd[0], &c, 1) != 1) e(0);
	}

	/* With the pipe empty, the good descriptor is not ready.  The bad
	 * one is an event all the same, so the call must come back with it
	 * at once rather than wait for the other -- here, forever. */
	memset(pfd, 0, sizeof(pfd));
	pfd[0].fd = pipefd[0];
	pfd[0].events = POLLIN;
	pfd[1].fd = bad;
	pfd[1].events = POLLIN;

	guard(5);
	if (poll(pfd, 2, -1) != 1) e(0);
	alarm(0);
	if (pfd[0].revents != 0) e(0);
	if (pfd[1].revents != POLLNVAL) e(0);

	/* The same with a timeout given, which must not be waited out. */
	memset(pfd, 0, sizeof(pfd));
	pfd[0].fd = pipefd[0];
	pfd[0].events = POLLIN;
	pfd[1].fd = bad;
	pfd[1].events = POLLIN;

	guard(5);
	if (poll(pfd, 2, 30000) != 1) e(0);
	alarm(0);
	if (pfd[1].revents != POLLNVAL) e(0);

	/* Nothing but negative descriptors: a plain sleep, which must still
	 * honour its timeout rather than return at once. */
	memset(pfd, 0, sizeof(pfd));
	pfd[0].fd = -1;
	pfd[0].events = POLLIN;
	guard(5);
	if (poll(pfd, 1, 100) != 0) e(0);
	alarm(0);
	if (pfd[0].revents != 0) e(0);
}

#define WAIT_USECS	200000		/* time for a child to get ready */

/*
 * How full a pipe has to be before it stops being writable, and that a
 * select(2) waiting for that is actually woken.
 */
static void
test99e(void)
{
	static char buf[PIPE_BUF];
	struct timeval tv;
	fd_set wr;
	int fd[2], flags, i, n, status;
	pid_t pid;

	subtest = 5;

	if (pipe(fd) != 0) e(0);

	/* An empty pipe is writable. */
	if (writable(fd[1], 0) != 1) e(0);

	/* So is one with something in it but room for a whole write -- the
	 * state a pipe of exactly PIPE_BUF bytes could never be in.
	 */
	if (write(fd[1], buf, 1) != 1) e(0);
	if (writable(fd[1], 0) != 1) e(0);
	if (read(fd[0], buf, 1) != 1) e(0);

	/* Find out how much it holds, counted in atomic writes.  This has to
	 * be more than one, or "is there room for a whole write" would be
	 * the same question as "is the pipe empty".
	 */
	if ((flags = fcntl(fd[1], F_GETFL)) == -1) e(0);
	if (fcntl(fd[1], F_SETFL, flags | O_NONBLOCK) != 0) e(0);

	errno = 0;
	for (n = 0; write(fd[1], buf, PIPE_BUF) == PIPE_BUF; n++)
		/* nothing */;
	if (errno != EAGAIN) e(0);
	if (n < 2) efmt("pipe holds %d atomic writes, want at least 2", n);

	if (fcntl(fd[1], F_SETFL, flags) != 0) e(0);

	/* A full pipe is not writable. */
	if (writable(fd[1], 0) != 0) e(0);

	/* Taking one byte out does not make room for an atomic write. */
	if (read(fd[0], buf, 1) != 1) e(0);
	if (writable(fd[1], 0) != 0) e(0);

	/* Taking out the rest of one does. */
	if (read(fd[0], buf, PIPE_BUF - 1) != PIPE_BUF - 1) e(0);
	if (writable(fd[1], 0) != 1) e(0);

	/* Empty the pipe again: it holds n writes, one of which is gone. */
	for (i = 1; i < n; i++)
		if (read(fd[0], buf, PIPE_BUF) != PIPE_BUF) e(0);
	if (writable(fd[1], 0) != 1) e(0);

	/* A blocking wait is woken by the reader that makes room -- not one
	 * read later, and not never.
	 */
	for (i = 0; i < n; i++)
		if (write(fd[1], buf, PIPE_BUF) != PIPE_BUF) e(0);
	if (writable(fd[1], 0) != 0) e(0);

	switch (pid = fork()) {
	case -1:
		e(0);
		break;
	case 0:
		usleep(WAIT_USECS);
		if (read(fd[0], buf, PIPE_BUF) != PIPE_BUF) exit(1);
		exit(0);
	default:
		break;
	}

	FD_ZERO(&wr);
	FD_SET(fd[1], &wr);
	tv.tv_sec = 5;
	tv.tv_usec = 0;
	guard(10);
	if (select(fd[1] + 1, NULL, &wr, NULL, &tv) != 1) e(0);
	alarm(0);
	if (!FD_ISSET(fd[1], &wr)) e(0);

	if (waitpid(pid, &status, 0) != pid) e(0);
	if (!WIFEXITED(status) || WEXITSTATUS(status) != 0) e(0);

	/* Fill it up again for the next case: the child took one write. */
	if (write(fd[1], buf, PIPE_BUF) != PIPE_BUF) e(0);
	if (writable(fd[1], 0) != 0) e(0);

	/* The other thing that makes a full pipe ready is the last reader
	 * going away: a write then fails with EPIPE rather than blocking.
	 */
	switch (pid = fork()) {
	case -1:
		e(0);
		break;
	case 0:
		usleep(WAIT_USECS);
		exit(0);		/* closes the last read end */
	default:
		break;
	}

	if (close(fd[0]) != 0) e(0);	/* the child holds the only one now */

	FD_ZERO(&wr);
	FD_SET(fd[1], &wr);
	tv.tv_sec = 5;
	tv.tv_usec = 0;
	guard(10);
	if (select(fd[1] + 1, NULL, &wr, NULL, &tv) != 1) e(0);
	alarm(0);

	if (waitpid(pid, &status, 0) != pid) e(0);
	if (!WIFEXITED(status) || WEXITSTATUS(status) != 0) e(0);

	if (close(fd[1]) != 0) e(0);
}

/*
 * A write larger than the pipe has to be split up, which is the one place
 * the pipe's size arithmetic is exercised for real: what comes out the far
 * end must be all of it, in order.
 */
#define BIG_SIZE	65536

static void
test99f(void)
{
	static unsigned char buf[BIG_SIZE];
	size_t off, i;
	ssize_t n;
	int fd[2], status;
	pid_t pid;

	subtest = 6;

	if (pipe(fd) != 0) e(0);

	for (i = 0; i < BIG_SIZE; i++)
		buf[i] = (unsigned char)(i % 251);

	switch (pid = fork()) {
	case -1:
		e(0);
		break;
	case 0:
		if (close(fd[0]) != 0) exit(1);
		if (write(fd[1], buf, BIG_SIZE) != BIG_SIZE) exit(2);
		if (close(fd[1]) != 0) exit(3);
		exit(0);
	default:
		break;
	}

	if (close(fd[1]) != 0) e(0);

	memset(buf, 0, sizeof(buf));
	for (off = 0; off < BIG_SIZE; off += (size_t)n) {
		guard(30);
		n = read(fd[0], buf + off, BIG_SIZE - off);
		alarm(0);
		if (n <= 0) {
			efmt("read %zd at offset %zu", n, off);
			break;
		}
	}
	if (off != BIG_SIZE) e(0);

	/* And nothing after it. */
	guard(30);
	if (read(fd[0], buf, 1) != 0) e(0);
	alarm(0);

	for (i = 0; i < BIG_SIZE; i++)
		if (buf[i] != (unsigned char)(i % 251)) {
			efmt("byte %zu is %u, want %u", i, buf[i],
			    (unsigned)(i % 251));
			break;
		}

	if (close(fd[0]) != 0) e(0);

	if (waitpid(pid, &status, 0) != pid) e(0);
	if (!WIFEXITED(status)) e(0);
	if (WEXITSTATUS(status) != 0) efmt("writer exited %d",
	    WEXITSTATUS(status));
}

int
main(int argc, char **argv)
{
	int i, m;

	start(99);

	if (pipe(pipefd) != 0) e(0);

	m = (argc == 2) ? atoi(argv[1]) : 0x3F;

	for (i = 0; i < 2; i++) {
		if (m & 0x01) test99a();
		if (m & 0x02) test99b();
		if (m & 0x04) test99c();
		if (m & 0x10) test99e();
		if (m & 0x20) test99f();
	}

	if (close(pipefd[0]) != 0) e(0);
	if (close(pipefd[1]) != 0) e(0);

	quit();

	return 0;
}
