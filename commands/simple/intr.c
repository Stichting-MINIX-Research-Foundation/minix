/*	intr 1.4 - run a command with interrupts enabled
 *							Author: Kees J. Bot
 *								17 Dec 1992
 */
#define nil 0
#ifndef _POSIX_SOURCE
#define _POSIX_SOURCE 1
#endif
#include <sys/types.h>
#include <errno.h>
#include <fcntl.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#if __minix
static char DEV_LOG[]= "/dev/log";
#else
static char DEV_LOG[]= "/dev/console";
#endif

static void say(const char *s)
{
	write(2, s, strlen(s));
}

static void fatal(const char *label)
{
	int err= errno;

	say("intr: ");
	say(label);
	say(": ");
	say(strerror(err));
	say("\n");
	exit(1);
}

static void usage(void)
{
	say("Usage: intr [-d] [-t seconds] command [arg ...]\n");
	exit(1);
}

int main(int argc, char **argv)
{
	int fd;
	unsigned n= 0;
	int daemonize= 0;
	int i;

	i= 1;
	while (i < argc && argv[i][0] == '-') {
		char *opt= argv[i++]+1, *end;
		unsigned long sec;

		if (opt[0] == '-' && opt[1] == 0) break;

		while (*opt != 0) switch (*opt++) {
		case 'd':
			/* -d */
			daemonize= 1;
			break;
		case 't':
			/* -t n: alarm in n seconds. */
			if (*opt == 0) {
				if (i == argc) usage();
				opt= argv[i++];
			}
			sec= strtoul(opt, &end, 10);
			if (end == opt || *end != 0 || (n= sec) != sec)
				usage();
			opt= "";
			break;
		default:
			usage();
		}
	}

	if ((argc - i) < 1) usage();

	/* Try to open the controlling tty. */
	if ((fd= open("/dev/tty", O_RDWR)) < 0) {
		if (errno != ENXIO) fatal("/dev/tty");
	}

	if (!daemonize) {
		/* Bring to the foreground.  If we already have a controlling
		 * tty then use it.  Otherwise try to allocate the console as
		 * controlling tty and begin a process group.
		 */
		if (fd < 0) {
			if (setsid() < 0) fatal("setsid()");

			fd= open("/dev/console", O_RDWR);
		}

		if (fd >= 0) {
			if (fd != 0) {
				dup2(fd, 0);
				close(fd);
			}
			dup2(0, 1);
			dup2(0, 2);
		}

		/* Set the usual signals back to the default. */
		signal(SIGHUP, SIG_DFL);
		signal(SIGINT, SIG_DFL);
		signal(SIGQUIT, SIG_DFL);
		signal(SIGTERM, SIG_DFL);
	} else {
		/* Send to the background.  Redirect input to /dev/null, and
		 * output to the log device.  Detach from the process group.
		 */
		if (fd >= 0) {
			close(fd);

			if (setsid() < 0) fatal("setsid()");
		}
		if ((fd= open("/dev/null", O_RDWR)) < 0) fatal("/dev/null");
		if (fd != 0) {
			dup2(fd, 0);
			close(fd);
		}
		if ((fd= open(DEV_LOG, O_WRONLY)) < 0) fatal(DEV_LOG);
		if (fd != 1) {
			dup2(fd, 1);
			close(fd);
		}
		dup2(1, 2);

		/* Move to the root directory. */
		(void) chdir("/");
	}

	/* Schedule the alarm.  (It is inherited over execve.) */
	if (n != 0) alarm(n);

	/* Call program. */
	execvp(argv[i], argv + i);

	/* Complain. */
	fatal(argv[i]);
	return 0;
}
