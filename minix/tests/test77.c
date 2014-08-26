/* Tests for opening/closing pseudo terminals - by D.C. van Moolenbroek */
/* This test needs to be run as root; otherwise, openpty() won't work. */
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <signal.h>
#include <termios.h>
#include <sys/wait.h>
#include <sys/syslimits.h>
#include <paths.h>
#include <fcntl.h>
#include <util.h>

#define ITERATIONS 10

#include "common.h"

static int sighups;		/* number of SIGHUP signals received */

/*
 * Signal handler for SIGHUP and SIGUSR1.
 */
static void
signal_handler(int sig)
{
	if (sig == SIGHUP)
		sighups++;
}

/*
 * Set the slave side of the pseudo terminal to raw mode.  This simplifies
 * testing communication.
 */
static void
make_raw(int slavefd)
{
	struct termios tios;

	if (tcgetattr(slavefd, &tios) < 0) e(100);

	cfmakeraw(&tios);

	if (tcsetattr(slavefd, TCSANOW, &tios) < 0) e(101);
}

/*
 * See if the given pseudo terminal can successfully perform basic
 * communication between master and slave.
 */
static void
test_comm(int masterfd, int slavefd)
{
	char c;

	make_raw(slavefd);

	c = 'A';
	if (write(masterfd, &c, sizeof(c)) != sizeof(c)) e(200);
	if (read(slavefd, &c, sizeof(c)) != sizeof(c)) e(201);
	if (c != 'A') e(202);

	c = 'B';
	if (write(slavefd, &c, sizeof(c)) != sizeof(c)) e(203);
	if (read(masterfd, &c, sizeof(c)) != sizeof(c)) e(204);
	if (c != 'B') e(205);

	c = 'C';
	if (write(masterfd, &c, sizeof(c)) != sizeof(c)) e(206);
	if (read(slavefd, &c, sizeof(c)) != sizeof(c)) e(207);
	if (c != 'C') e(208);

	c = 'D';
	if (write(slavefd, &c, sizeof(c)) != sizeof(c)) e(209);
	if (read(masterfd, &c, sizeof(c)) != sizeof(c)) e(210);
	if (c != 'D') e(211);
}

/*
 * Get device node names for the master and slave end of a free pseudo
 * terminal.  We don't want to replicate the entire openpty(3) logic here, so
 * start by letting openpty(3) do the work for us.  We make the assumption that
 * nobody snatches the pair while we are running.
 */
static void
get_names(char pname[PATH_MAX], char tname[PATH_MAX])
{
	int len, masterfd, slavefd;

	if (openpty(&masterfd, &slavefd, tname, NULL, NULL) < 0) e(300);

	/*
	 * openpty(3) gives us only the slave name, but we also need the master
	 * name.
	 */
	strlcpy(pname, tname, PATH_MAX);
	len = strlen(_PATH_DEV);

	if (strncmp(pname, _PATH_DEV, len)) e(301);

	/* If this fails, this test needs to be updated. */
	if (pname[len] != 't') e(302);

	pname[len] = 'p';

	test_comm(masterfd, slavefd);

	if (close(masterfd) < 0) e(303);
	if (close(slavefd) < 0) e(304);
}

/*
 * Test various orders of opening and closing the master and slave sides of a
 * pseudo terminal, as well as opening/closing one side without ever opening
 * the other.
 */
static void
test77a(void)
{
	struct sigaction act, oact;
	char pname[PATH_MAX], tname[PATH_MAX];
	int masterfd, slavefd;

	subtest = 1;

	/* We do not want to get SIGHUP signals in this test. */
	memset(&act, 0, sizeof(act));
	act.sa_handler = SIG_IGN;
	if (sigaction(SIGHUP, &act, &oact) < 0) e(1);

	/* Get master and slave device names for a free pseudo terminal. */
	get_names(pname, tname);

	/* Try opening and then closing the master. */
	if ((masterfd = open(pname, O_RDWR | O_NOCTTY)) < 0) e(2);

	if (close(masterfd) < 0) e(3);

	/* Now see if we can reopen the master as well as the slave. */
	if ((masterfd = open(pname, O_RDWR | O_NOCTTY)) < 0) e(4);
	if ((slavefd = open(tname, O_RDWR | O_NOCTTY)) < 0) e(5);

	test_comm(masterfd, slavefd);

	/* In the meantime, test different closing orders. This is order A. */
	if (close(slavefd) < 0) e(6);
	if (close(masterfd) < 0) e(7);

	/* Now try opening the pair again. */
	if ((masterfd = open(pname, O_RDWR | O_NOCTTY)) < 0) e(8);
	if ((slavefd = open(tname, O_RDWR | O_NOCTTY)) < 0) e(9);

	test_comm(masterfd, slavefd);

	if (close(slavefd) < 0) e(10);

	/*
	 * Try reopening the slave after closing it.  It is not very important
	 * that this works, but the TTY driver should currently support it.
	 */
	if ((slavefd = open(tname, O_RDWR | O_NOCTTY)) < 0) e(11);

	test_comm(masterfd, slavefd);

	/* This is closing order B. This may or may not cause a SIGHUP. */
	if (close(masterfd) < 0) e(12);
	if (close(slavefd) < 0) e(13);

	/* Try the normal open procedure. */
	if ((masterfd = open(pname, O_RDWR | O_NOCTTY)) < 0) e(14);
	if ((slavefd = open(tname, O_RDWR | O_NOCTTY)) < 0) e(15);

	test_comm(masterfd, slavefd);

	if (close(slavefd) < 0) e(16);
	if (close(masterfd) < 0) e(17);

	/* Try reopening and closing the slave, without opening the master. */
	if ((slavefd = open(tname, O_RDWR | O_NOCTTY)) < 0) e(18);

	if (close(slavefd) < 0) e(19);

	/* Again, try the normal open procedure. */
	if ((masterfd = open(pname, O_RDWR | O_NOCTTY)) < 0) e(20);
	if ((slavefd = open(tname, O_RDWR | O_NOCTTY)) < 0) e(21);

	test_comm(masterfd, slavefd);

	if (close(slavefd) < 0) e(22);
	if (close(masterfd) < 0) e(23);

	/* Finally, try opening the slave first. */
	if ((slavefd = open(tname, O_RDWR | O_NOCTTY)) < 0) e(24);
	if ((masterfd = open(pname, O_RDWR | O_NOCTTY)) < 0) e(25);

	test_comm(masterfd, slavefd);

	if (close(slavefd) < 0) e(26);
	if (close(masterfd) < 0) e(27);

	if (sigaction(SIGHUP, &oact, NULL) < 0) e(28);
}

/*
 * Test opening a single side multiple times.
 */
static void
test77b(void)
{
	char pname[PATH_MAX], tname[PATH_MAX];
	int masterfd, slavefd, extrafd;

	subtest = 2;

	/* Get master and slave device names for a free pseudo terminal. */
	get_names(pname, tname);

	/* It must not be possible to open the master multiple times. */
	if ((masterfd = open(pname, O_RDWR | O_NOCTTY)) < 0) e(1);
	if ((slavefd = open(tname, O_RDWR | O_NOCTTY)) < 0) e(2);

	test_comm(masterfd, slavefd);

	if ((extrafd = open(pname, O_RDWR | O_NOCTTY)) >= 0) e(3);
	if (errno != EIO) e(4);

	test_comm(masterfd, slavefd);

	if (close(slavefd) < 0) e(5);
	if (close(masterfd) < 0) e(6);

	/* The slave can be opened multiple times, though. */
	if ((masterfd = open(pname, O_RDWR | O_NOCTTY)) < 0) e(7);
	if ((slavefd = open(tname, O_RDWR | O_NOCTTY)) < 0) e(8);

	test_comm(masterfd, slavefd);

	if ((extrafd = open(tname, O_RDWR | O_NOCTTY)) < 0) e(9);

	test_comm(masterfd, extrafd);
	test_comm(masterfd, slavefd);

	if (close(slavefd) < 0) e(10);
	if (close(extrafd) < 0) e(11);
	if (close(masterfd) < 0) e(12);
}

/*
 * Test communication on half-open pseudo terminals.
 */
static void
test77c(void)
{
	struct sigaction act, oact;
	char pname[PATH_MAX], tname[PATH_MAX];
	int masterfd, slavefd;
	char c;

	subtest = 3;

	/* We do not want to get SIGHUP signals in this test. */
	memset(&act, 0, sizeof(act));
	act.sa_handler = SIG_IGN;
	if (sigaction(SIGHUP, &act, &oact) < 0) e(1);

	/* Get master and slave device names for a free pseudo terminal. */
	get_names(pname, tname);

	if ((masterfd = open(pname, O_RDWR | O_NOCTTY)) < 0) e(2);

	/* Writes to the master should be buffered until there is a slave. */
	c = 'E';
	if (write(masterfd, &c, sizeof(c)) != sizeof(c)) e(3);

	if ((slavefd = open(tname, O_RDWR | O_NOCTTY)) < 0) e(4);

	make_raw(slavefd);

	if (read(slavefd, &c, sizeof(c)) != sizeof(c)) e(5);
	if (c != 'E') e(6);

	/* Discard the echo on the master. */
	if (tcflush(slavefd, TCOFLUSH) != 0) e(7);

	test_comm(masterfd, slavefd);

	if (close(slavefd) < 0) e(8);

	/* Writes to the master after the slave has been closed should fail. */
	if (write(masterfd, &c, sizeof(c)) >= 0) e(9);
	if (errno != EIO) e(10);

	if (close(masterfd) < 0) e(11);

	/* Writes to the slave should be buffered until there is a master. */
	if ((slavefd = open(tname, O_RDWR | O_NOCTTY)) < 0) e(12);

	make_raw(slavefd);

	c = 'F';
	if (write(slavefd, &c, sizeof(c)) != sizeof(c)) e(13);

	if ((masterfd = open(pname, O_RDWR | O_NOCTTY)) < 0) e(14);

	if (read(masterfd, &c, sizeof(c)) != sizeof(c)) e(15);
	if (c != 'F') e(16);

	test_comm(masterfd, slavefd);

	if (close(masterfd) < 0) e(17);

	if (write(slavefd, &c, sizeof(c)) >= 0) e(18);
	if (errno != EIO) e(19);

	/* Reads from the slave should return EOF if the master is gone. */
	if (read(slavefd, &c, sizeof(c)) != 0) e(20);

	if (close(slavefd) < 0) e(21);

	if (sigaction(SIGHUP, &oact, NULL) < 0) e(22);
}

/*
 * Wait for a child process to terminate.  Return 0 if the child exited without
 * errors, -1 otherwise.
 */
static int
waitchild(void)
{
	int status;

	if (wait(&status) <= 0) return -1;
	if (!WIFEXITED(status)) return -1;
	if (WEXITSTATUS(status) != 0) return -1;

	return 0;
}

/*
 * Test opening the slave side with and without the O_NOCTTY flag.
 */
static void
test77d(void)
{
	char pname[PATH_MAX], tname[PATH_MAX];
	int masterfd, slavefd;

	subtest = 4;

	/* Get master and slave device names for a free pseudo terminal. */
	get_names(pname, tname);

	/* Make ourselves process group leader if we aren't already. */
	(void) setsid();

	if ((masterfd = open(pname, O_RDWR | O_NOCTTY)) < 0) e(1);

	/*
	 * Opening the slave with O_NOCTTY should not change its controlling
	 * terminal.
	 */
	switch (fork()) {
	case 0:
		if (setsid() < 0) e(2);

		if ((slavefd = open(tname, O_RDWR | O_NOCTTY)) < 0) e(3);

		if (open("/dev/tty", O_RDWR) >= 0) e(4);
		if (errno != ENXIO) e(5);

		exit(errct);
	case -1:
		e(6);
	default:
		break;
	}

	if (waitchild() < 0) e(7);

	if (close(masterfd) < 0) e(8);

	if ((masterfd = open(pname, O_RDWR | O_NOCTTY)) < 0) e(9);

	/*
	 * Opening the slave without O_NOCTTY should change its controlling
	 * terminal, though.
	 */
	switch (fork()) {
	case 0:
		if (setsid() < 0) e(10);

		if ((slavefd = open(tname, O_RDWR)) < 0) e(11);

		if (open("/dev/tty", O_RDWR) < 0) e(12);

		exit(errct);
	case -1:
		e(13);
	default:
		break;
	}

	if (waitchild() < 0) e(14);

	if (close(masterfd) < 0) e(15);
}

/*
 * Test receiving of SIGHUP on master hang-up.  All of the tests so far have
 * ignored SIGHUP, and probably would not have received one anyway, since the
 * process was not its own session leader.  Time to test this aspect.
 */
static void
test77e(void)
{
	struct sigaction act, hup_oact, usr_oact;
	sigset_t set, oset;
	char pname[PATH_MAX], tname[PATH_MAX];
	int masterfd, slavefd;

	subtest = 5;

	/* Get master and slave device names for a free pseudo terminal. */
	get_names(pname, tname);

	memset(&act, 0, sizeof(act));
	act.sa_handler = signal_handler;
	if (sigaction(SIGHUP, &act, &hup_oact) < 0) e(1);

	memset(&act, 0, sizeof(act));
	act.sa_handler = signal_handler;
	if (sigaction(SIGUSR1, &act, &usr_oact) < 0) e(2);

	sigemptyset(&set);
	sigaddset(&set, SIGHUP);
	sigaddset(&set, SIGUSR1);
	if (sigprocmask(SIG_BLOCK, &set, &oset) < 0) e(3);

	sighups = 0;

	/* Make ourselves process group leader if we aren't already. */
	(void) setsid();

	if ((masterfd = open(pname, O_RDWR | O_NOCTTY)) < 0) e(4);

	switch (fork()) {
	case 0:
		if (close(masterfd) < 0) e(5);

		/* Become session leader. */
		if (setsid() < 0) e(6);

		if ((slavefd = open(tname, O_RDWR)) < 0) e(7);

		/* Tell the parent we are ready. */
		kill(getppid(), SIGUSR1);

		/* We should now get a SIGHUP. */
		set = oset;
		if (sigsuspend(&set) >= 0) e(8);

		if (sighups != 1) e(9);

		exit(errct);
	case -1:
		e(10);
	default:
		break;
	}

	/* Wait for SIGUSR1 from the child. */
	set = oset;
	if (sigsuspend(&set) >= 0) e(11);

	/* Closing the master should now raise a SIGHUP signal in the child. */
	if (close(masterfd) < 0) e(12);

	if (waitchild() < 0) e(13);

	if (sigprocmask(SIG_SETMASK, &oset, NULL) < 0) e(14);

	if (sigaction(SIGHUP, &hup_oact, NULL) < 0) e(15);
	if (sigaction(SIGUSR1, &usr_oact, NULL) < 0) e(16);
}

/*
 * Test basic select functionality on /dev/tty.  While this test should not be
 * part of this test set, we already have all the infrastructure we need here.
 */
static void
test77f(void)
{
	struct sigaction act, oact;
	char c, pname[PATH_MAX], tname[PATH_MAX];
	struct timeval tv;
	fd_set fd_set;
	int fd, maxfd, masterfd, slavefd;

	subtest = 6;

	/* We do not want to get SIGHUP signals in this test. */
	memset(&act, 0, sizeof(act));
	act.sa_handler = SIG_IGN;
	if (sigaction(SIGHUP, &act, &oact) < 0) e(1);

	/* Get master and slave device names for a free pseudo terminal. */
	get_names(pname, tname);

	if ((masterfd = open(pname, O_RDWR | O_NOCTTY)) < 0) e(2);

	switch (fork()) {
	case 0:
		if (setsid() < 0) e(3);

		close(masterfd);

		if ((slavefd = open(tname, O_RDWR)) < 0) e(4);

		if ((fd = open("/dev/tty", O_RDWR)) < 0) e(5);

		make_raw(fd);

		/* Without slave input, /dev/tty is not ready for reading. */
		FD_ZERO(&fd_set);
		FD_SET(fd, &fd_set);
		tv.tv_sec = 0;
		tv.tv_usec = 0;

		if (select(fd + 1, &fd_set, NULL, NULL, &tv) != 0) e(6);
		if (FD_ISSET(fd, &fd_set)) e(7);

		FD_SET(fd, &fd_set);
		tv.tv_sec = 0;
		tv.tv_usec = 10000;

		if (select(fd + 1, &fd_set, NULL, NULL, &tv) != 0) e(8);
		if (FD_ISSET(fd, &fd_set)) e(9);

		/* It will be ready for writing, though. */
		FD_SET(fd, &fd_set);

		if (select(fd + 1, NULL, &fd_set, NULL, NULL) != 1) e(10);
		if (!FD_ISSET(fd, &fd_set)) e(11);

		/* Test mixing file descriptors to the same terminal. */
		FD_ZERO(&fd_set);
		FD_SET(fd, &fd_set);
		FD_SET(slavefd, &fd_set);
		tv.tv_sec = 0;
		tv.tv_usec = 10000;

		maxfd = fd > slavefd ? fd : slavefd;
		if (select(maxfd + 1, &fd_set, NULL, NULL, &tv) != 0) e(12);
		if (FD_ISSET(fd, &fd_set)) e(13);
		if (FD_ISSET(slavefd, &fd_set)) e(14);

		/* The delayed echo on the master must wake up our select. */
		c = 'A';
		if (write(slavefd, &c, sizeof(c)) != sizeof(c)) e(15);

		FD_ZERO(&fd_set);
		FD_SET(fd, &fd_set);

		if (select(fd + 1, &fd_set, NULL, NULL, NULL) != 1) e(16);
		if (!FD_ISSET(fd, &fd_set)) e(17);

		/* Select must now still flag readiness for reading. */
		tv.tv_sec = 0;
		tv.tv_usec = 0;

		if (select(fd + 1, &fd_set, NULL, NULL, &tv) != 1) e(18);
		if (!FD_ISSET(fd, &fd_set)) e(19);

		/* That is, until we read the byte. */
		if (read(slavefd, &c, sizeof(c)) != sizeof(c)) e(20);
		if (c != 'B') e(21);

		if (select(fd + 1, &fd_set, NULL, NULL, &tv) != 0) e(22);
		if (FD_ISSET(fd, &fd_set)) e(23);

		/* Ask the parent to close the master. */
		c = 'C';
		if (write(slavefd, &c, sizeof(c)) != sizeof(c)) e(24);

		FD_SET(fd, &fd_set);

		/* The closure must cause an EOF condition on the slave. */
		if (select(fd + 1, &fd_set, NULL, NULL, NULL) != 1) e(25);
		if (!FD_ISSET(fd, &fd_set)) e(26);

		if (select(fd + 1, &fd_set, NULL, NULL, NULL) != 1) e(27);
		if (!FD_ISSET(fd, &fd_set)) e(28);

		if (read(slavefd, &c, sizeof(c)) != 0) e(29);

		exit(errct);
	case -1:
		e(30);
	default:
		/* Wait for the child to write something to the slave. */
		FD_ZERO(&fd_set);
		FD_SET(masterfd, &fd_set);

		if (select(masterfd + 1, &fd_set, NULL, NULL, NULL) != 1)
			e(31);
		if (!FD_ISSET(masterfd, &fd_set)) e(32);

		if (read(masterfd, &c, sizeof(c)) != sizeof(c)) e(33);
		if (c != 'A') e(34);

		/* Write a reply once the child is blocked in its select. */
		tv.tv_sec = 1;
		tv.tv_usec = 0;
		if (select(masterfd + 1, &fd_set, NULL, NULL, &tv) != 0)
			e(35);

		c = 'B';
		if (write(masterfd, &c, sizeof(c)) != sizeof(c)) e(36);

		/* Wait for the child to request closing the master. */
		if (read(masterfd, &c, sizeof(c)) != sizeof(c)) e(37);
		if (c != 'C') e(38);

		/* Close the master once the child is blocked in its select. */
		sleep(1);

		close(masterfd);

		break;
	}

	if (waitchild() < 0) e(39);

	if (sigaction(SIGHUP, &oact, NULL) < 0) e(28);
}

int
main(int argc, char **argv)
{
	int i, m;

	start(77);

	if (argc == 2)
		m = atoi(argv[1]);
	else
		m = 0xFF;

	for (i = 0; i < ITERATIONS; i++) {
		if (m & 0x01) test77a();
		if (m & 0x02) test77b();
		if (m & 0x04) test77c();
		if (m & 0x08) test77d();
		if (m & 0x10) test77e();
		if (m & 0x20) test77f();
	}

	quit();
}
