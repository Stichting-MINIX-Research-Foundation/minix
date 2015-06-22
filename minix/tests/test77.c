/* Tests for opening/closing pseudo terminals - by D.C. van Moolenbroek */
/*
 * As of the introduction of Unix98 PTY support, this test set actually relies
 * on the ability to create Unix98 PTYs.  The system still supports old-style
 * PTYs but there is no way to force openpty(3) to use them.  However, part of
 * this test set can still be used to test old-style PTYs: first disable Unix98
 * PTYs, for example by unmounting PTYFS or temporarily removing /dev/ptmx, and
 * then run the a-f subtests from this test set as root.
 */
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <signal.h>
#include <termios.h>
#include <sys/wait.h>
#include <sys/syslimits.h>
#include <paths.h>
#include <dirent.h>
#include <grp.h>
#include <fcntl.h>
#include <util.h>

#define ITERATIONS 10

#define MIN_PTYS 4

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

	if (tcgetattr(slavefd, &tios) < 0) e(0);

	cfmakeraw(&tios);

	if (tcsetattr(slavefd, TCSANOW, &tios) < 0) e(0);
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
	if (write(masterfd, &c, sizeof(c)) != sizeof(c)) e(0);
	if (read(slavefd, &c, sizeof(c)) != sizeof(c)) e(0);
	if (c != 'A') e(0);

	c = 'B';
	if (write(slavefd, &c, sizeof(c)) != sizeof(c)) e(0);
	if (read(masterfd, &c, sizeof(c)) != sizeof(c)) e(0);
	if (c != 'B') e(0);

	c = 'C';
	if (write(masterfd, &c, sizeof(c)) != sizeof(c)) e(0);
	if (read(slavefd, &c, sizeof(c)) != sizeof(c)) e(0);
	if (c != 'C') e(0);

	c = 'D';
	if (write(slavefd, &c, sizeof(c)) != sizeof(c)) e(0);
	if (read(masterfd, &c, sizeof(c)) != sizeof(c)) e(0);
	if (c != 'D') e(0);
}

/*
 * Obtain a pseudo terminal.  The master end is opened and its file descriptor
 * stored in 'pfd'.  The slave path name is stored in 'tname'.  For old-style
 * PTYs, the function returns 1 and stores the master name in 'pname' if not
 * NULL.  For Unix98 PTYs, the function returns 0, in which case no master name
 * is available.  For old-style PTYs, the caller may close and reopen the
 * master.  In that case, we make the assumption that nobody snatches the pair
 * while we are running.  For Unix98 PTYs, the master must be kept open.
 */
static int
get_pty(int *pfd, char pname[PATH_MAX], char tname[PATH_MAX])
{
	char *name;
	int len, masterfd, slavefd;

	/*
	 * First try Unix98 PTY allocation, mainly to avoid opening the slave
	 * end immediately.  If this fails, try openpty(3) as well.
	 */
	if ((masterfd = posix_openpt(O_RDWR | O_NOCTTY)) != -1) {
		if (grantpt(masterfd) != -1 && unlockpt(masterfd) != -1 &&
		    (name = ptsname(masterfd)) != NULL) {
			*pfd = masterfd;
			strlcpy(tname, name, PATH_MAX);

			return 0;
		}
		if (close(masterfd) < 0) e(0);
	}

	if (openpty(&masterfd, &slavefd, tname, NULL, NULL) < 0) e(0);

	test_comm(masterfd, slavefd);

	*pfd = masterfd;

	if (close(slavefd) < 0) e(0);

	/*
	 * openpty(3) gives us only the slave name, but we also want the master
	 * name.
	 */
	len = strlen(_PATH_DEV);
	if (strncmp(tname, _PATH_DEV, len)) e(0);

	if (strncmp(&tname[len], "tty", 3))
		return 0; /* Unix98 after all?  Well okay, whatever.. */

	if (pname != NULL) {
		strlcpy(pname, tname, PATH_MAX);
		pname[len] = 'p';
	}

	return 1;
}

/*
 * Test various orders of opening and closing the master and slave sides of a
 * pseudo terminal, as well as opening/closing one side without ever opening
 * the other.  This test is meaningful mainly for old-style pseudoterminals.
 */
static void
test77a(void)
{
	struct sigaction act, oact;
	char pname[PATH_MAX], tname[PATH_MAX];
	int oldstyle, masterfd, slavefd;

	subtest = 1;

	/* We do not want to get SIGHUP signals in this test. */
	memset(&act, 0, sizeof(act));
	act.sa_handler = SIG_IGN;
	if (sigaction(SIGHUP, &act, &oact) < 0) e(0);

	/* Obtain a pseudo terminal. */
	oldstyle = get_pty(&masterfd, pname, tname);

	if (oldstyle) {
		/* Try closing the master. */
		if (close(masterfd) < 0) e(0);

		/* See if we can reopen the master. */
		if ((masterfd = open(pname, O_RDWR | O_NOCTTY)) < 0) e(0);
	}

	if ((slavefd = open(tname, O_RDWR | O_NOCTTY)) < 0) e(0);

	test_comm(masterfd, slavefd);

	/* In the meantime, test different closing orders. This is order A. */
	if (close(slavefd) < 0) e(0);
	if (close(masterfd) < 0) e(0);

	/* Now try opening the pair (or a new pair) again. */
	if (!oldstyle)
		oldstyle = get_pty(&masterfd, pname, tname);
	else
		if ((masterfd = open(pname, O_RDWR | O_NOCTTY)) < 0) e(0);

	if ((slavefd = open(tname, O_RDWR | O_NOCTTY)) < 0) e(0);

	test_comm(masterfd, slavefd);

	if (close(slavefd) < 0) e(0);

	/*
	 * Try reopening the slave after closing it.  It is not very important
	 * that this works, but the TTY driver should currently support it.
	 */
	if ((slavefd = open(tname, O_RDWR | O_NOCTTY)) < 0) e(0);

	test_comm(masterfd, slavefd);

	/* This is closing order B. This may or may not cause a SIGHUP. */
	if (close(masterfd) < 0) e(0);
	if (close(slavefd) < 0) e(0);

	/* Try the normal open procedure. */
	if (!oldstyle)
		oldstyle = get_pty(&masterfd, pname, tname);
	else
		if ((masterfd = open(pname, O_RDWR | O_NOCTTY)) < 0) e(0);

	if ((slavefd = open(tname, O_RDWR | O_NOCTTY)) < 0) e(0);

	test_comm(masterfd, slavefd);

	if (close(slavefd) < 0) e(0);
	if (close(masterfd) < 0) e(0);

	/*
	 * Try reopening and closing the slave, without opening the master.
	 * This should work on old-style PTYS, but not on Unix98 PTYs.
	 */
	if ((slavefd = open(tname, O_RDWR | O_NOCTTY)) >= 0) {
		if (!oldstyle) e(0);

		if (close(slavefd) < 0) e(0);
	} else
		if (oldstyle) e(0);

	/* Again, try the normal open procedure. */
	if (!oldstyle)
		oldstyle = get_pty(&masterfd, pname, tname);
	else
		if ((masterfd = open(pname, O_RDWR | O_NOCTTY)) < 0) e(0);

	if ((slavefd = open(tname, O_RDWR | O_NOCTTY)) < 0) e(0);

	test_comm(masterfd, slavefd);

	if (close(slavefd) < 0) e(0);
	if (close(masterfd) < 0) e(0);

	/*
	 * Finally, try opening the slave first.  This does not work with
	 * Unix98 PTYs.
	 */
	if (oldstyle) {
		if ((slavefd = open(tname, O_RDWR | O_NOCTTY)) < 0) e(0);
		if ((masterfd = open(pname, O_RDWR | O_NOCTTY)) < 0) e(0);

		test_comm(masterfd, slavefd);

		if (close(slavefd) < 0) e(0);
		if (close(masterfd) < 0) e(0);
	}

	if (sigaction(SIGHUP, &oact, NULL) < 0) e(0);
}

/*
 * Test opening a single side multiple times.
 */
static void
test77b(void)
{
	char pname[PATH_MAX], tname[PATH_MAX];
	int oldstyle, masterfd, slavefd, extrafd;

	subtest = 2;

	/* Obtain a pseudo terminal. */
	oldstyle = get_pty(&masterfd, pname, tname);

	if ((slavefd = open(tname, O_RDWR | O_NOCTTY)) < 0) e(0);

	/*
	 * It must not be possible to open the master multiple times.  Doing so
	 * is possible only if we have a named master, i.e., an old-style PTY.
	 */
	test_comm(masterfd, slavefd);

	if (oldstyle) {
		if ((extrafd = open(pname, O_RDWR | O_NOCTTY)) >= 0) e(0);
		if (errno != EIO) e(0);
	}

	test_comm(masterfd, slavefd);

	if (close(slavefd) < 0) e(0);
	if (close(masterfd) < 0) e(0);

	/* The slave can be opened multiple times, though. */
	oldstyle = get_pty(&masterfd, pname, tname);

	if ((slavefd = open(tname, O_RDWR | O_NOCTTY)) < 0) e(0);

	test_comm(masterfd, slavefd);

	if ((extrafd = open(tname, O_RDWR | O_NOCTTY)) < 0) e(0);

	test_comm(masterfd, extrafd);
	test_comm(masterfd, slavefd);

	if (close(slavefd) < 0) e(0);
	if (close(extrafd) < 0) e(0);
	if (close(masterfd) < 0) e(0);
}

/*
 * Test communication on half-open pseudo terminals.
 */
static void
test77c(void)
{
	struct sigaction act, oact;
	char pname[PATH_MAX], tname[PATH_MAX];
	int oldstyle, masterfd, slavefd;
	char c;

	subtest = 3;

	/* We do not want to get SIGHUP signals in this test. */
	memset(&act, 0, sizeof(act));
	act.sa_handler = SIG_IGN;
	if (sigaction(SIGHUP, &act, &oact) < 0) e(0);

	/* Obtain a pseudo terminal. */
	oldstyle = get_pty(&masterfd, pname, tname);

	/*
	 * For old-style pseudo terminals, we have just opened and closed the
	 * slave end, which alters the behavior we are testing below.  Close
	 * and reopen the master to start fresh.
	 */
	if (oldstyle) {
		if (close(masterfd) < 0) e(0);

		if ((masterfd = open(pname, O_RDWR | O_NOCTTY)) < 0) e(0);
	}

	/* Writes to the master should be buffered until there is a slave. */
	c = 'E';
	if (write(masterfd, &c, sizeof(c)) != sizeof(c)) e(0);

	if ((slavefd = open(tname, O_RDWR | O_NOCTTY)) < 0) e(0);

	make_raw(slavefd);

	if (read(slavefd, &c, sizeof(c)) != sizeof(c)) e(0);
	if (c != 'E') e(0);

	/* Discard the echo on the master. */
	if (tcflush(slavefd, TCOFLUSH) != 0) e(0);

	test_comm(masterfd, slavefd);

	if (close(slavefd) < 0) e(0);

	/* Writes to the master after the slave has been closed should fail. */
	if (write(masterfd, &c, sizeof(c)) >= 0) e(0);
	if (errno != EIO) e(0);

	if (oldstyle)
		if (close(masterfd) < 0) e(0);

	/*
	 * Writes to the slave should be buffered until there is a master.
	 * This applies to old-style PTYs only.
	 */
	if ((slavefd = open(tname, O_RDWR | O_NOCTTY)) < 0) e(0);

	if (oldstyle) {
		make_raw(slavefd);

		c = 'F';
		if (write(slavefd, &c, sizeof(c)) != sizeof(c)) e(0);

		if ((masterfd = open(pname, O_RDWR | O_NOCTTY)) < 0) e(0);

		if (read(masterfd, &c, sizeof(c)) != sizeof(c)) e(0);
		if (c != 'F') e(0);
	}

	test_comm(masterfd, slavefd);

	if (close(masterfd) < 0) e(0);

	if (write(slavefd, &c, sizeof(c)) >= 0) e(0);
	if (errno != EIO) e(0);

	/* Reads from the slave should return EOF if the master is gone. */
	if (read(slavefd, &c, sizeof(c)) != 0) e(0);

	if (close(slavefd) < 0) e(0);

	if (sigaction(SIGHUP, &oact, NULL) < 0) e(0);
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

	/* Make ourselves process group leader if we aren't already. */
	(void)setsid();

	/* Obtain a pseudo terminal. */
	(void)get_pty(&masterfd, NULL, tname);

	/*
	 * Opening the slave with O_NOCTTY should not change its controlling
	 * terminal.
	 */
	switch (fork()) {
	case 0:
		if (close(masterfd) < 0) e(0);

		if (setsid() < 0) e(0);

		if ((slavefd = open(tname, O_RDWR | O_NOCTTY)) < 0) e(0);

		if (open("/dev/tty", O_RDWR) >= 0) e(0);
		if (errno != ENXIO) e(0);

		exit(errct);
	case -1:
		e(0);
	default:
		break;
	}

	if (waitchild() < 0) e(0);

	if (close(masterfd) < 0) e(0);

	(void)get_pty(&masterfd, pname, tname);

	/*
	 * Opening the slave without O_NOCTTY should change its controlling
	 * terminal, though.
	 */
	switch (fork()) {
	case 0:
		if (close(masterfd) < 0) e(0);

		if (setsid() < 0) e(0);

		if ((slavefd = open(tname, O_RDWR)) < 0) e(0);

		if (open("/dev/tty", O_RDWR) < 0) e(0);

		exit(errct);
	case -1:
		e(0);
	default:
		break;
	}

	if (waitchild() < 0) e(0);

	if (close(masterfd) < 0) e(0);
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
	char tname[PATH_MAX];
	int masterfd, slavefd;

	subtest = 5;

	memset(&act, 0, sizeof(act));
	act.sa_handler = signal_handler;
	if (sigaction(SIGHUP, &act, &hup_oact) < 0) e(0);

	memset(&act, 0, sizeof(act));
	act.sa_handler = signal_handler;
	if (sigaction(SIGUSR1, &act, &usr_oact) < 0) e(0);

	sigemptyset(&set);
	sigaddset(&set, SIGHUP);
	sigaddset(&set, SIGUSR1);
	if (sigprocmask(SIG_BLOCK, &set, &oset) < 0) e(0);

	sighups = 0;

	/* Make ourselves process group leader if we aren't already. */
	(void)setsid();

	/* Obtain a pseudo terminal. */
	(void)get_pty(&masterfd, NULL, tname);

	switch (fork()) {
	case 0:
		if (close(masterfd) < 0) e(0);

		/* Become session leader. */
		if (setsid() < 0) e(0);

		if ((slavefd = open(tname, O_RDWR)) < 0) e(0);

		/* Tell the parent we are ready. */
		kill(getppid(), SIGUSR1);

		/* We should now get a SIGHUP. */
		set = oset;
		if (sigsuspend(&set) >= 0) e(0);

		if (sighups != 1) e(0);

		exit(errct);
	case -1:
		e(0);
	default:
		break;
	}

	/* Wait for SIGUSR1 from the child. */
	set = oset;
	if (sigsuspend(&set) >= 0) e(0);

	/* Closing the master should now raise a SIGHUP signal in the child. */
	if (close(masterfd) < 0) e(0);

	if (waitchild() < 0) e(0);

	if (sigprocmask(SIG_SETMASK, &oset, NULL) < 0) e(0);

	if (sigaction(SIGHUP, &hup_oact, NULL) < 0) e(0);
	if (sigaction(SIGUSR1, &usr_oact, NULL) < 0) e(0);
}

/*
 * Test basic select functionality on /dev/tty.  While this test should not be
 * part of this test set, we already have all the infrastructure we need here.
 */
static void
test77f(void)
{
	struct sigaction act, oact;
	char c, tname[PATH_MAX];
	struct timeval tv;
	fd_set fd_set;
	int fd, maxfd, masterfd, slavefd;

	subtest = 6;

	/* We do not want to get SIGHUP signals in this test. */
	memset(&act, 0, sizeof(act));
	act.sa_handler = SIG_IGN;
	if (sigaction(SIGHUP, &act, &oact) < 0) e(0);

	/* Obtain a pseudo terminal. */
	(void)get_pty(&masterfd, NULL, tname);

	switch (fork()) {
	case 0:
		if (close(masterfd) < 0) e(0);

		if (setsid() < 0) e(0);

		if ((slavefd = open(tname, O_RDWR)) < 0) e(0);

		if ((fd = open("/dev/tty", O_RDWR)) < 0) e(0);

		make_raw(fd);

		/* Without slave input, /dev/tty is not ready for reading. */
		FD_ZERO(&fd_set);
		FD_SET(fd, &fd_set);
		tv.tv_sec = 0;
		tv.tv_usec = 0;

		if (select(fd + 1, &fd_set, NULL, NULL, &tv) != 0) e(0);
		if (FD_ISSET(fd, &fd_set)) e(0);

		FD_SET(fd, &fd_set);
		tv.tv_sec = 0;
		tv.tv_usec = 10000;

		if (select(fd + 1, &fd_set, NULL, NULL, &tv) != 0) e(0);
		if (FD_ISSET(fd, &fd_set)) e(0);

		/* It will be ready for writing, though. */
		FD_SET(fd, &fd_set);

		if (select(fd + 1, NULL, &fd_set, NULL, NULL) != 1) e(0);
		if (!FD_ISSET(fd, &fd_set)) e(0);

		/* Test mixing file descriptors to the same terminal. */
		FD_ZERO(&fd_set);
		FD_SET(fd, &fd_set);
		FD_SET(slavefd, &fd_set);
		tv.tv_sec = 0;
		tv.tv_usec = 10000;

		maxfd = fd > slavefd ? fd : slavefd;
		if (select(maxfd + 1, &fd_set, NULL, NULL, &tv) != 0) e(0);
		if (FD_ISSET(fd, &fd_set)) e(0);
		if (FD_ISSET(slavefd, &fd_set)) e(0);

		/* The delayed echo on the master must wake up our select. */
		c = 'A';
		if (write(slavefd, &c, sizeof(c)) != sizeof(c)) e(0);

		FD_ZERO(&fd_set);
		FD_SET(fd, &fd_set);

		if (select(fd + 1, &fd_set, NULL, NULL, NULL) != 1) e(0);
		if (!FD_ISSET(fd, &fd_set)) e(0);

		/* Select must now still flag readiness for reading. */
		tv.tv_sec = 0;
		tv.tv_usec = 0;

		if (select(fd + 1, &fd_set, NULL, NULL, &tv) != 1) e(0);
		if (!FD_ISSET(fd, &fd_set)) e(0);

		/* That is, until we read the byte. */
		if (read(slavefd, &c, sizeof(c)) != sizeof(c)) e(0);
		if (c != 'B') e(0);

		if (select(fd + 1, &fd_set, NULL, NULL, &tv) != 0) e(0);
		if (FD_ISSET(fd, &fd_set)) e(0);

		/* Ask the parent to close the master. */
		c = 'C';
		if (write(slavefd, &c, sizeof(c)) != sizeof(c)) e(0);

		FD_SET(fd, &fd_set);

		/* The closure must cause an EOF condition on the slave. */
		if (select(fd + 1, &fd_set, NULL, NULL, NULL) != 1) e(0);
		if (!FD_ISSET(fd, &fd_set)) e(0);

		if (select(fd + 1, &fd_set, NULL, NULL, NULL) != 1) e(0);
		if (!FD_ISSET(fd, &fd_set)) e(0);

		if (read(slavefd, &c, sizeof(c)) != 0) e(0);

		exit(errct);
	case -1:
		e(0);
	default:
		/* Wait for the child to write something to the slave. */
		FD_ZERO(&fd_set);
		FD_SET(masterfd, &fd_set);

		if (select(masterfd + 1, &fd_set, NULL, NULL, NULL) != 1)
			e(0);
		if (!FD_ISSET(masterfd, &fd_set)) e(0);

		if (read(masterfd, &c, sizeof(c)) != sizeof(c)) e(0);
		if (c != 'A') e(0);

		/* Write a reply once the child is blocked in its select. */
		tv.tv_sec = 1;
		tv.tv_usec = 0;
		if (select(masterfd + 1, &fd_set, NULL, NULL, &tv) != 0)
			e(0);

		c = 'B';
		if (write(masterfd, &c, sizeof(c)) != sizeof(c)) e(0);

		/* Wait for the child to request closing the master. */
		if (read(masterfd, &c, sizeof(c)) != sizeof(c)) e(0);
		if (c != 'C') e(0);

		/* Close the master once the child is blocked in its select. */
		sleep(1);

		close(masterfd);

		break;
	}

	if (waitchild() < 0) e(0);

	if (sigaction(SIGHUP, &oact, NULL) < 0) e(0);
}

/*
 * See if the directory contents of /dev/pts are as we expect.  We have to keep
 * in mind that other programs may have pseudo terminals open while we are
 * running, although we assume that those programs do not open or close PTYs
 * while we are running.
 */
static void
test_getdents(int nindex, int array[3], int present[3])
{
	struct group *group;
	DIR *dirp;
	struct dirent *dp;
	struct stat buf;
	char path[PATH_MAX], *endp;
	gid_t tty_gid;
	int i, n, seen_dot, seen_dotdot, seen_index[3], *seen;

	seen_dot = seen_dotdot = 0;
	for (i = 0; i < nindex; i++)
		seen_index[i] = 0;

	if ((group = getgrnam("tty")) == NULL) e(0);
	tty_gid = group->gr_gid;

	if ((dirp = opendir(_PATH_DEV_PTS)) == NULL) e(0);

	while ((dp = readdir(dirp)) != NULL) {
		snprintf(path, sizeof(path), _PATH_DEV_PTS "%s", dp->d_name);
		if (stat(path, &buf) < 0) e(0);

		if (!strcmp(dp->d_name, ".") || !strcmp(dp->d_name, "..")) {
			seen =
			    (dp->d_name[1] == '.') ? &seen_dot : &seen_dotdot;
			if (*seen) e(0);
			*seen = 1;

			/* Check basic dirent and stat fields. */
			if (dp->d_type != DT_DIR) e(0);
			if (dp->d_name[1] == '\0' &&
			    buf.st_ino != dp->d_fileno) e(0);
			if (!S_ISDIR(buf.st_mode)) e(0);
			if (buf.st_nlink < 2) e(0);
		} else {
			/* The file name must be a number. */
			errno = 0;
			n = strtol(dp->d_name, &endp, 10);
			if (errno != 0) e(0);
			if (dp->d_name[0] == '\0' || *endp != '\0') e(0);
			if (n < 0) e(0);

			/* Check basic dirent and stat fields. */
			if (dp->d_type != DT_CHR) e(0);
			if (buf.st_ino != dp->d_fileno) e(0);
			if (!S_ISCHR(buf.st_mode)) e(0);
			if (buf.st_nlink != 1) e(0);
			if (buf.st_size != 0) e(0);
			if (buf.st_rdev == 0) e(0);

			/* Is this one of the PTYs we created? */
			for (i = 0; i < nindex; i++) {
				if (array[i] == n) {
					if (seen_index[i]) e(0);
					seen_index[i] = 1;

					break;
				}
			}

			/* If so, perform some extra tests. */
			if (i < nindex) {
				if ((buf.st_mode & ALLPERMS) != 0620) e(0);
				if (buf.st_uid != getuid()) e(0);
				if (buf.st_gid != tty_gid) e(0);
			}
		}
	}

	if (closedir(dirp) < 0) e(0);

	if (!seen_dot) e(0);
	if (!seen_dotdot) e(0);
	for (i = 0; i < nindex; i++)
		if (seen_index[i] != present[i]) e(0);
}

/*
 * Obtain a Unix98 PTY.  Return an open file descriptor for the master side,
 * and store the name of the slave side in 'tptr'.
 */
static int
get_unix98_pty(char ** tptr)
{
	int masterfd;

	if ((masterfd = posix_openpt(O_RDWR | O_NOCTTY)) < 0) e(0);

	if (grantpt(masterfd) < 0) e(0);

	/* This call is a no-op on MINIX3. */
	if (unlockpt(masterfd) < 0) e(0);

	if ((*tptr = ptsname(masterfd)) == NULL) e(0);

	return masterfd;
}

/*
 * Test for Unix98 PTY support and PTYFS.
 */
static void
test77g(void)
{
	char *tname;
	struct stat buf;
	size_t len;
	int i, masterfd, slavefd, fd[3], array[3], present[3];

	subtest = 7;

	/*
	 * Test basic operation, and verify that the slave node disappears
	 * after both sides of the pseudo terminal have been closed.  We check
	 * different combinations of open master and slave ends (with 'i'):
	 * 0) opening and closing the master only, 1) closing a slave before
	 * the master, and 2) closing the slave after the master.
	 */
	for (i = 0; i <= 2; i++) {
		masterfd = get_unix98_pty(&tname);

		if (access(tname, R_OK | W_OK) < 0) e(0);

		if (i > 0) {
			if ((slavefd = open(tname, O_RDWR | O_NOCTTY)) < 0)
				e(0);

			if (access(tname, R_OK | W_OK) < 0) e(0);

			if (i > 1) {
				if (close(masterfd) < 0) e(0);

				masterfd = slavefd; /* ugly but saving code */
			} else
				if (close(slavefd) < 0) e(0);
		}

		if (access(tname, R_OK | W_OK) < 0) e(0);

		if (close(masterfd) < 0) e(0);

		if (access(tname, R_OK | W_OK) == 0) e(0);
	}

	/*
	 * Test whether we can open multiple pseudo terminals.  We need to be
	 * able to open three PTYs.  Verify that they are properly listed in
	 * the /dev/pts directory contents, and have proper attributes set.
	 */
	test_getdents(0, NULL, NULL);

	for (i = 0; i < 3; i++) {
		fd[i] = get_unix98_pty(&tname);

		/* Figure out the slave index number. */
		len = strlen(_PATH_DEV_PTS);
		if (strncmp(tname, _PATH_DEV_PTS, strlen(_PATH_DEV_PTS))) e(0);
		array[i] = atoi(&tname[len]);
		present[i] = 1;
	}

	test_getdents(3, array, present);

	if (close(fd[0]) < 0) e(0);
	present[0] = 0;

	test_getdents(3, array, present);

	if (close(fd[2]) < 0) e(0);
	present[2] = 0;

	test_getdents(3, array, present);

	if (close(fd[1]) < 0) e(0);
	present[1] = 0;

	test_getdents(3, array, present);

	/*
	 * Test chmod(2) on a slave node, and multiple calls to grantpt(3).
	 * The first grantpt(3) call should create the slave node (we currently
	 * can not test this: the slave node may be created earlier as well,
	 * but we do not know its name), whereas subsequent grantpt(3) calls
	 * should reset its mode, uid, and gid.  Testing the latter two and
	 * chown(2) on the slave node requires root, so we skip that part.
	 *
	 * Finally, NetBSD revokes access to existing slave file descriptors
	 * upon a call to grantpt(3).  This is not a POSIX requirement, but
	 * NetBSD needs this for security reasons because it already creates
	 * the slave node when the master is opened (and it does not lock the
	 * slave until a call to unlockpt(3)).  MINIX3 does not implement
	 * revocation this way, because the slave node is created only upon the
	 * call to grantpt(3), thus leaving no insecure window for the slave
	 * side between posix_openpt(3) and grantpt(3).  While this behavior
	 * may be changed later, we test for the lack of revocation here now.
	 */
	masterfd = get_unix98_pty(&tname);

	if ((slavefd = open(tname, O_RDWR | O_NOCTTY)) < 0) e(0);

	if (stat(tname, &buf) != 0) e(0);
	if (buf.st_mode != (S_IFCHR | 0620)) e(0);

	if (chmod(tname, S_IFCHR | 0630) != 0) e(0);

	if (stat(tname, &buf) != 0) e(0);
	if (buf.st_mode != (S_IFCHR | 0630)) e(0);

	if (grantpt(masterfd) != 0) e(0);

	if (stat(tname, &buf) != 0) e(0);
	if (buf.st_mode != (S_IFCHR | 0620)) e(0);

	test_comm(masterfd, slavefd);

	if (close(slavefd) < 0) e(0);
	if (close(masterfd) < 0) e(0);

	test_getdents(0, NULL, NULL);
}

/*
 * Check that the given PTY index, which is in use for an old-style PTY, is not
 * allocated through Unix98 PTY allocation.  This test is not foolproof, but it
 * does the job well enough.
 */
static void
test_overlap(int m)
{
	char *tname;
	size_t len;
	int i, n, fd[MIN_PTYS];

	for (i = 0; i < MIN_PTYS; i++) {
		if ((fd[i] = posix_openpt(O_RDWR | O_NOCTTY)) < 0)
			break; /* out of PTYs */
		if (grantpt(fd[i]) < 0) e(0);
		if (unlockpt(fd[i]) < 0) e(0);
		if ((tname = ptsname(fd[i])) == NULL) e(0);

		len = strlen(_PATH_DEV_PTS);
		if (strncmp(tname, _PATH_DEV_PTS, strlen(_PATH_DEV_PTS))) e(0);
		n = atoi(&tname[len]);
		if (n < 0 || n > 9) e(0);

		if (m == n) e(0);
	}

	for (i--; i >= 0; i--)
		if (close(fd[i]) < 0) e(0);
}

/*
 * Test for mixing access to old-style and Unix98 PTYs.  Since the PTY service
 * internally shares the set of pseudo terminals between the two types, it has
 * to implement checks to prevent that a PTY opened as one type is also
 * accessed through the other type.  We test some of those checks here.
 */
static void
test77h(void)
{
	char *tname, ptest[PATH_MAX], ttest[PATH_MAX];
	struct sigaction act, oact;
	size_t len;
	int i, n, masterfd, slavefd;

	subtest = 8;

	/* We do not want to get SIGHUP signals in this test. */
	memset(&act, 0, sizeof(act));
	act.sa_handler = SIG_IGN;
	if (sigaction(SIGHUP, &act, &oact) < 0) e(0);

	/*
	 * Check that Unix98 PTYs cannot be accessed through old-style device
	 * nodes.  We check different combinations of open master and
	 * slave ends for the Unix98 side (with 'i'): 0) opening and closing
	 * the master only, 1) closing a slave before the master, and 2)
	 * closing the slave after the master.
	 *
	 * This test relies on the implementation aspect that /dev/ttypN and
	 * /dev/pts/N (with N in the range 0..9) map to the same PTY.  It also
	 * relies on lack of concurrent PTY allocation outside the test.
	 */
	for (i = 0; i <= 2; i++) {
		/* Open a Unix98 PTY and get the slave name. */
		masterfd = get_unix98_pty(&tname);

		/* Figure out the slave index number. */
		len = strlen(_PATH_DEV_PTS);
		if (strncmp(tname, _PATH_DEV_PTS, strlen(_PATH_DEV_PTS))) e(0);
		n = atoi(&tname[len]);
		if (n < 0 || n > 9) e(0);

		/* Use this index number to create old-style device names. */
		snprintf(ptest, sizeof(ptest), _PATH_DEV "ptyp%u", n);
		snprintf(ttest, sizeof(ttest), _PATH_DEV "ttyp%u", n);

		/*
		 * Now make sure that opening the old-style master and slave
		 * fails as long as either side of the Unix98 PTY is open.
		 */
		if (open(ptest, O_RDWR | O_NOCTTY) >= 0) e(0);
		if (errno != EACCES && errno != EIO) e(0);
		if (open(ttest, O_RDWR | O_NOCTTY) >= 0) e(0);
		if (errno != EACCES && errno != EIO) e(0);

		if (i > 0) {
			if ((slavefd = open(tname, O_RDWR | O_NOCTTY)) < 0)
				e(0);

			if (open(ptest, O_RDWR | O_NOCTTY) >= 0) e(0);
			if (errno != EACCES && errno != EIO) e(0);
			if (open(ttest, O_RDWR | O_NOCTTY) >= 0) e(0);
			if (errno != EACCES && errno != EIO) e(0);

			if (close(slavefd) < 0) e(0);

			if (i > 1) {
				if (open(ptest, O_RDWR | O_NOCTTY) >= 0) e(0);
				if (errno != EACCES && errno != EIO) e(0);
				if (open(ttest, O_RDWR | O_NOCTTY) >= 0) e(0);
				if (errno != EACCES && errno != EIO) e(0);

				if ((slavefd =
				    open(tname, O_RDWR | O_NOCTTY)) < 0) e(0);

				if (open(ptest, O_RDWR | O_NOCTTY) >= 0) e(0);
				if (errno != EACCES && errno != EIO) e(0);
				if (open(ttest, O_RDWR | O_NOCTTY) >= 0) e(0);
				if (errno != EACCES && errno != EIO) e(0);

				if (close(masterfd) < 0) e(0);

				masterfd = slavefd; /* ugly but saving code */
			}

			if (open(ptest, O_RDWR | O_NOCTTY) >= 0) e(0);
			if (errno != EACCES && errno != EIO) e(0);
			if (open(ttest, O_RDWR | O_NOCTTY) >= 0) e(0);
			if (errno != EACCES && errno != EIO) e(0);
		}

		if (close(masterfd) < 0) e(0);

		/*
		 * Once both Unix98 sides are closed, the pseudo terminal can
		 * be reused.  Thus, opening the old-style master should now
		 * succeed.  However, it is possible that we do not have
		 * permission to open the master at all.
		 */
		if ((masterfd = open(ptest, O_RDWR | O_NOCTTY)) < 0 &&
		    errno != EACCES) e(0);

		if (masterfd >= 0 && close(masterfd) < 0) e(0);
	}

	/*
	 * The reverse test, which would check that old-style PTYs cannot be
	 * accessed through Unix98 device nodes, is impossible to perform
	 * properly without root privileges, as we would have to create device
	 * nodes manually with mknod(2).  All we can do here is ensure that if
	 * an old-style PTY is opened, it will not also be allocated as a
	 * Unix98 PTY.  We do a rather basic check, but only if we can open an
	 * old-style master at all.  We check two closing orders (with 'i'):
	 * 0) the slave first, 1) the master first.  Here, we make the hard
	 * assumption that the system supports at least four pseudo terminals,
	 * of which at least one is currently free.
	 */
	for (i = 0; i <= 1; i++) {
		for (n = 0; n < MIN_PTYS; n++) {
			snprintf(ptest, sizeof(ptest), _PATH_DEV "ptyp%u", n);

			if ((masterfd = open(ptest, O_RDWR | O_NOCTTY)) >= 0)
				break;
		}

		if (n >= MIN_PTYS)
			break;

		test_overlap(n);

		snprintf(ttest, sizeof(ttest), _PATH_DEV "ttyp%u", n);

		/* We can do part of the test only if we can open the slave. */
		if ((slavefd = open(ttest, O_RDWR | O_NOCTTY)) >= 0) {
			test_overlap(n);

			if (i > 0) {
				if (close(masterfd) < 0) e(0);

				masterfd = slavefd; /* again, ugly */
			} else
				if (close(slavefd) < 0) e(0);

			test_overlap(n);
		}

		if (close(masterfd) < 0) e(0);
	}

	if (sigaction(SIGHUP, &oact, NULL) < 0) e(0);
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
		if (m & 0x40) test77g();
		if (m & 0x80) test77h();
	}

	quit();
}
