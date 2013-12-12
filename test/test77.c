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
void
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

	if (close(slavefd) < 0) e(20);

	if (sigaction(SIGHUP, &oact, NULL) < 0) e(21);
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

		exit(0);
	case -1:
		e(6);
	default:
		break;
	}

	if (wait(NULL) <= 0) e(7);

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

		exit(0);
	case -1:
		e(13);
	default:
		break;
	}

	if (wait(NULL) <= 0) e(14);

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

		exit(0);
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

	if (wait(NULL) <= 0) e(13);

	if (sigprocmask(SIG_SETMASK, &oset, NULL) < 0) e(14);

	if (sigaction(SIGHUP, &hup_oact, NULL) < 0) e(15);
	if (sigaction(SIGUSR1, &usr_oact, NULL) < 0) e(16);
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
	}

	quit();
}
