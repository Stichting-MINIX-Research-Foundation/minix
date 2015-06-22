/* Tests for interrupting VFS calls - by D.C. van Moolenbroek */
#include <stdio.h>
#include <sys/time.h>
#include <sys/wait.h>
#include <sys/socket.h>
#include <sys/utsname.h>
#include <sys/syslimits.h>
#include <netinet/in.h>
#include <signal.h>
#include <unistd.h>
#include <fcntl.h>
#include <errno.h>
#include <util.h>

#define ITERATIONS 1

#include "common.h"

/*
 * This signal handler does nothing.  It just needs to be triggered, so that
 * PM will tell VFS to unpause this process.
 */
static void dummy_handler(int sig)
{
	/* Nothing. */
}

/*
 * Interrupt a select(2) call.
 */
static void
test76a(void)
{
	struct sigaction act, oact;
	struct itimerval it;
	struct sockaddr_in sin;
	struct timeval tv;
	fd_set set;
	int tfd[2], pfd[2], sfd, maxfd;

	subtest = 1;

	act.sa_handler = dummy_handler;
	sigfillset(&act.sa_mask);
	act.sa_flags = 0;
	if (sigaction(SIGALRM, &act, &oact) < 0) e(1);

	memset(&it, 0, sizeof(it));
	it.it_value.tv_sec = 0;
	it.it_value.tv_usec = 100000;
	if (setitimer(ITIMER_REAL, &it, NULL) < 0) e(2);

	/* First try without any file descriptors. */
	tv.tv_sec = 1;
	tv.tv_usec = 0;
	if (select(0, NULL, NULL, NULL, &tv) >= 0) e(3);
	if (errno != EINTR) e(4);

	/* Then try with different types of file descriptors, all blocking. */
	if (openpty(&tfd[0], &tfd[1], NULL, NULL, NULL) < 0) e(5);

	FD_ZERO(&set);
	FD_SET(tfd[0], &set); /* reading from the PTY master should block */
	maxfd = tfd[0];

	if (pipe(pfd) < 0) e(6);
	FD_SET(pfd[0], &set); /* reading from an empty pipe should block */
	if (maxfd < pfd[0]) maxfd = pfd[0];

	if ((sfd = socket(AF_INET, SOCK_STREAM, 0)) < 0) e(7);

	memset(&sin, 0, sizeof(sin));
	sin.sin_family = AF_INET;
	/* Binding to an arbitrary port is fine. */
	if (bind(sfd, (struct sockaddr *)&sin, sizeof(sin)) < 0) e(8);

	if (listen(sfd, 1) < 0) e(9);

	FD_SET(sfd, &set); /* reading from a listening socket should block */
	if (maxfd < sfd) maxfd = sfd;

	memset(&it, 0, sizeof(it));
	it.it_value.tv_sec = 0;
	it.it_value.tv_usec = 100000;
	if (setitimer(ITIMER_REAL, &it, NULL) < 0) e(10);

	tv.tv_sec = 1;
	tv.tv_usec = 0;
	if (select(maxfd + 1, &set, NULL, NULL, &tv) >= 0) e(11);
	if (errno != EINTR) e(12);

	if (close(tfd[0]) < 0) e(13);
	if (close(tfd[1]) < 0) e(14);
	if (close(pfd[0]) < 0) e(15);
	if (close(pfd[1]) < 0) e(16);
	if (close(sfd) < 0) e(17);

	if (sigaction(SIGUSR1, &oact, NULL) < 0) e(18);
}

/*
 * Interrupt reads and writes to a pipe.  POSIX states that if the operation
 * was partially successful, the number of bytes written so far should be
 * returned; otherwise, the we should get the normal EINTR.
 */
static void
test76b(void)
{
	struct sigaction act, oact;
	struct itimerval it;
	char *buf;
	int pfd[2];

	subtest = 2;

	if ((buf = malloc(PIPE_BUF * 2)) == NULL) e(1);

	if (pipe(pfd) < 0) e(2);

	act.sa_handler = dummy_handler;
	sigfillset(&act.sa_mask);
	act.sa_flags = 0;
	if (sigaction(SIGALRM, &act, &oact) < 0) e(3);

	memset(&it, 0, sizeof(it));
	it.it_value.tv_sec = 0;
	it.it_value.tv_usec = 100000;
	if (setitimer(ITIMER_REAL, &it, NULL) < 0) e(4);

	/*
	 * This write is too large for the pipe, so it will block until the
	 * signal arrives.  When being interrupted, it should return the pipe
	 * size, as that is the part that has been filled successfully so far.
	 */
	if (write(pfd[1], buf, PIPE_BUF * 2) != PIPE_BUF) e(5);

	/*
	 * Since the write partially succeeded, we should be able to read all
	 * we wrote so far, without blocking.
	 */
	if (read(pfd[0], buf, PIPE_BUF) != PIPE_BUF) e(6);

	/* We should now be able to fill the pipe up to its full size again. */
	if (write(pfd[1], buf, PIPE_BUF) != PIPE_BUF) e(7);

	memset(&it, 0, sizeof(it));
	it.it_value.tv_sec = 0;
	it.it_value.tv_usec = 100000;
	if (setitimer(ITIMER_REAL, &it, NULL) < 0) e(8);

	/* Now interrupt a write attempt on a full pipe. */
	if (write(pfd[1], buf, 1) >= 0) e(9);
	if (errno != EINTR) e(10);

	/* Empty the pipe again. */
	if (read(pfd[0], buf, PIPE_BUF) != PIPE_BUF) e(11);

	memset(&it, 0, sizeof(it));
	it.it_value.tv_sec = 0;
	it.it_value.tv_usec = 100000;
	if (setitimer(ITIMER_REAL, &it, NULL) < 0) e(12);

	/* Now interrupt a read on an empty pipe. */
	if (read(pfd[0], buf, PIPE_BUF) >= 0) e(13);
	if (errno != EINTR) e(14);

	if (close(pfd[0]) < 0) e(15);
	if (close(pfd[1]) < 0) e(16);

	if (sigaction(SIGUSR1, &oact, NULL) < 0) e(17);

	free(buf);
}

/*
 * Interrupt an ioctl(2) call.  We use an alarm to interrupt an accept(3) call
 * on a TCP socket - the accept procedure is (currently) implemented using
 * ioctl(2) calls.
 */
static void
test76c(void)
{
	struct sigaction act, oact;
	struct itimerval it;
	struct sockaddr_in sin;
	socklen_t len;
	int sfd;

	subtest = 3;

	if ((sfd = socket(AF_INET, SOCK_STREAM, 0)) < 0) e(1);

	memset(&sin, 0, sizeof(sin));
	sin.sin_family = AF_INET;
	/* Binding to an arbitrary port is fine. */
	if (bind(sfd, (struct sockaddr *)&sin, sizeof(sin)) < 0) e(2);

	if (listen(sfd, 1) < 0) e(3);

	act.sa_handler = dummy_handler;
	sigfillset(&act.sa_mask);
	act.sa_flags = 0;
	if (sigaction(SIGALRM, &act, &oact) < 0) e(4);

	memset(&it, 0, sizeof(it));
	it.it_value.tv_sec = 0;
	it.it_value.tv_usec = 100000;
	if (setitimer(ITIMER_REAL, &it, NULL) < 0) e(5);

	/* This will block until the timer fires. */
	len = sizeof(sin);
	if (accept(sfd, (struct sockaddr *)&sin, &len) >= 0) e(6);
	if (errno != EINTR) e(7);

	if (close(sfd) < 0) e(8);

	if (sigaction(SIGUSR1, &oact, NULL) < 0) e(9);
}

/*
 * Try to trigger semi-concurrent processing of normal system calls and
 * postponed PM requests for a single process within VFS.
 */
static void
test76d(void)
{
	struct utsname name;
	struct sigaction act, oact;
	struct itimerval it;
	int r, fd, pfd[2], count, status;
	time_t stime, etime, runtime = 30 /*seconds*/;
	char buf[3], *pbuf;

	subtest = 4;

	/* This test would kill wimpy platforms such as ARM. */
	if (uname(&name) < 0) e(1);
	if (!strcmp(name.machine, "arm")) return;

	act.sa_handler = dummy_handler;
	sigfillset(&act.sa_mask);
	act.sa_flags = 0;
	if (sigaction(SIGALRM, &act, &oact) < 0) e(2);

	if (pipe(pfd) < 0) e(3);

	/* Pre-fill the pipe. */
	if ((pbuf = malloc(PIPE_BUF - 1)) == NULL) e(4);

	if (write(pfd[1], pbuf, PIPE_BUF - 1) != PIPE_BUF - 1) e(5);

	free(pbuf);

	switch (fork()) {
	case 0:
		if (close(pfd[1]) < 0) e(6);

		/* Read from the pipe, but more slowly than the writer. */
		while ((r = read(pfd[0], buf, 2)) != 0)
			if (r < 0) e(7);

		exit(0);
	case -1:
		e(8);
	default:
		break;
	}

	switch (fork()) {
	case 0:
		if (close(pfd[0]) < 0) e(9);

		time(&stime);

		/* Start an alarm mayhem. */
		it.it_value.tv_sec = 0;
		it.it_value.tv_usec = 1;
		it.it_interval.tv_sec = 0;
		it.it_interval.tv_usec = 1;
		if (setitimer(ITIMER_REAL, &it, NULL) < 0) e(10);

		/*
		 * Then start writing to the pipe, in such a way that the
		 * write operation will be suspended in every so many cases.
		 */
		do {
			if (write(pfd[1], buf, 3) < 0 && errno != EINTR)
				e(11);

			time(&etime);
		} while ((int)(etime - stime) < runtime);

		exit(0);
	case -1:
		e(12);
	default:
		break;
	}

	if (close(pfd[0]) < 0) e(13);
	if (close(pfd[1]) < 0) e(14);

	/*
	 * First give the two processes a while to run regularly.  Then start
	 * creating additional noise to keep the VFS worker threads busy.
	 */
	runtime /= 2;

	sleep(runtime);

	/*
	 * As of writing, VFS has less than 20 worker threads. Create more
	 * processes than that.
	 */
	for (count = 2; count < 20; count++) {
		switch (fork()) {
		case 0:
			time(&stime);

			do {
				/*
				 * Opening a character device blocks the
				 * calling thread, hopefully causing work to be
				 * queued.  Sadly, in practice, the high
				 * priorities of system processes prevent this
				 * case from occurring frequently.  It works
				 * better with a driver that has a priority
				 * below that of of user processes.
				 */
				if ((fd = open("/dev/null", O_WRONLY)) < 0)
					e(15);

				close(fd);

				time(&etime);
			} while ((int)(etime - stime) < runtime);

			exit(0);
		case -1:
			e(16);
		default:
			break;
		}
	}

	/* Wait for all children to shut down. */
	while (count-- > 0) {
		if (wait(&status) <= 0) e(17);
		if (!WIFEXITED(status)) e(18);
		if (WEXITSTATUS(status) != 0) e(19);
	}

	if (sigaction(SIGUSR1, &oact, NULL) < 0) e(20);
}

/*
 * Try to get a nonblocking select(2) call to be interrupted by a signal.
 * In the future, VFS should prevent this from happening at all; for now, we
 * just want to make sure it does not result in disaster when it does happen.
 */
static void
test76e(void)
{
	struct utsname name;
	struct sigaction act, oact;
	struct itimerval it;
	struct timeval tv;
	fd_set set;
	int tfd[2], left;

	subtest = 5;

	/* This test would kill wimpy platforms such as ARM. */
	if (uname(&name) < 0) e(1);
	if (!strcmp(name.machine, "arm")) return;

	if (openpty(&tfd[0], &tfd[1], NULL, NULL, NULL) < 0) e(2);

	act.sa_handler = dummy_handler;
	sigfillset(&act.sa_mask);
	act.sa_flags = 0;
	if (sigaction(SIGALRM, &act, &oact) < 0) e(3);

	/*
	 * Start an alarm mayhem.  We have to try to get a signal in between
	 * VFS sending a select request to TTY, and TTY replying to VFS with
	 * initial results.
	 */
	it.it_value.tv_sec = 0;
	it.it_value.tv_usec = 1;
	it.it_interval.tv_sec = 0;
	it.it_interval.tv_usec = 1;
	if (setitimer(ITIMER_REAL, &it, NULL) < 0) e(4);

	/*
	 * Now issue nonblocking selects until we get interrupted, or until
	 * we have gone through a hardcoded maximum of attempts.
	 */
	left = 100000;
	do {
		if (--left == 0) break;

		FD_ZERO(&set);
		FD_SET(tfd[0], &set); /* reading from master should block */

		tv.tv_sec = 0;
		tv.tv_usec = 0;
	} while (select(2, &set, NULL, NULL, &tv) >= 0);

	if (left > 0 && errno != EINTR) e(5);

	it.it_value.tv_sec = 0;
	it.it_value.tv_usec = 0;
	if (setitimer(ITIMER_REAL, &it, NULL) < 0) e(6);

	/* The call failed, so the set must be unmodified. */
	if (left > 0 && !FD_SET(tfd[0], &set)) e(7);

	if (close(tfd[0]) < 0) e(8);
	if (close(tfd[1]) < 0) e(9);

	if (sigaction(SIGUSR1, &oact, NULL) < 0) e(10);
}

int
main(int argc, char **argv)
{
	int i, m;

	start(76);

	if (argc == 2)
		m = atoi(argv[1]);
	else
		m = 0xFF;

	for (i = 0; i < ITERATIONS; i++) {
		if (m & 0x01) test76a();
		if (m & 0x02) test76b();
		if (m & 0x04) test76c();
		if (m & 0x08) test76d();
		if (m & 0x10) test76e();
	}

	quit();
}
