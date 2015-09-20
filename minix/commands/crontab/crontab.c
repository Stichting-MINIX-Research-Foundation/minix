/*	crontab 1.2 - user crontab manipulation		Author: Kees J. Bot
 *								12 Jan 1997
 */
#define nil ((void*)0)
#include <sys/types.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <signal.h>
#include <time.h>
#include <errno.h>
#include <unistd.h>
#include <fcntl.h>
#include <pwd.h>
#include <sys/stat.h>
#include "misc.h"
#include "tab.h"

static int opentab(int uid, char *file, int how)
/* Open a crontab file under the given uid.  How is 'r' or 'w'.  Return
 * the result of open(2).
 */
{
	uid_t safe_uid;
	int flags, r, err;

	switch (how) {
	case 'r':	flags= O_RDONLY;			break;
	case 'w':	flags= O_WRONLY | O_CREAT | O_TRUNC;	break;
	default:	errno= EINVAL;				return -1;
	}

	safe_uid= geteuid();
	seteuid(uid);
	r= open(file, flags, 0666);
	err= errno;
	seteuid(safe_uid);
	errno= err;
	return r;
}

static void copytab(int fd_in, char *file_in, int fd_out, char *file_out)
/* Copy one open file to another.  Complain and exit on errors. */
{
	ssize_t r, w;
	char buf[1024];

	while ((r= read(fd_in, buf, sizeof(buf))) > 0) {
		w= 0;
		while (w < r) {
			if ((r= write(fd_out, buf+w, r-w)) <= 0) {
				fprintf(stderr,
				"%s: Write error on %s: %s\n",
					prog_name,
					file_out,
					r == 0 ? "End of file"
							: strerror(errno));
				exit(1);
			}
			w+= r;
		}
	}
	if (r < 0) {
		fprintf(stderr, "%s: Read error on %s: %s\n",
			prog_name, file_in, strerror(errno));
		exit(1);
	}
}

static void usage(void)
{
	fprintf(stderr,
		"Usage: %s -c [user] file  # Change crontab\n"
		"       %s -l [user]       # List crontab\n"
		"       %s -r [user]       # Remove crontab\n"
		"       %s -p              # Tell cron to reload\n",
		prog_name, prog_name, prog_name, prog_name);
	exit(1);
}

int main(int argc, char **argv)
{
	int i;
	int cflag, lflag, rflag, pflag;
	uid_t uid;
	char *user, *file;
	struct passwd *pw;
	static char SPOOLDIR[]= "/usr/spool/crontabs";
	char tabfile[sizeof(SPOOLDIR) + NAME_MAX];

	prog_name= strrchr(argv[0], '/');
	if (prog_name == nil) prog_name= argv[0]; else prog_name++;

	cflag= lflag= rflag= pflag= 0;
	i= 1;
	while (i < argc && argv[i][0] == '-') {
		char *opt= argv[i++] + 1;

		if (opt[0] == '-' && opt[1] == 0) break;	/* -- */

		while (*opt != 0) switch (*opt++) {
		case 'c':	cflag= 1;	break;
		case 'l':	lflag= 1;	break;
		case 'r':	rflag= 1;	break;
		case 'p':	pflag= 1;	break;
		default:	usage();
		}
	}
	if (cflag + lflag + rflag + pflag != 1) usage();

	user= file= nil;
	if (!pflag && i < argc) user= argv[i++];
	if (cflag) {
		if (user == nil) usage();
		if (i < argc) {
			file= argv[i++];
		} else {
			file= user;
			user= nil;
		}
	}
	if (i != argc) usage();

	if (geteuid() != 0) {
		fprintf(stderr, "%s: No root privileges?\n", prog_name);
	}
	uid= getuid();
	if (user == nil) {
		if ((pw= getpwuid(uid)) == nil) {
			fprintf(stderr,
				"%s: Don't know who you (uid %lu) are!\n",
				prog_name, (unsigned long) uid);
			exit(1);
		}
	} else {
		if ((pw= getpwnam(user)) == nil) {
			fprintf(stderr,
				"%s: Don't know who you (%s) are!\n",
				prog_name, user);
			exit(1);
		}
	}
	if (uid != 0 && pw->pw_uid != uid) {
		fprintf(stderr,
		"%s: Only root can change the crontabs of others!\n",
			prog_name);
		exit(1);
	}
	user= pw->pw_name;
	uid= pw->pw_uid;
	seteuid(uid);
	umask(0077);

	selectlog(STDERR);
	sprintf(tabfile, "%s/%s", SPOOLDIR, user);

	if (lflag) {
		int fd;

		if ((fd= opentab(0, tabfile, 'r')) < 0) {
			fprintf(stderr, "%s: Can't open %s: %s\n",
				prog_name, tabfile, strerror(errno));
			exit(1);
		}
		copytab(fd, tabfile, 1, "stdout");
		close(fd);
	}

	if (rflag) {
		seteuid(0);
		if (unlink(tabfile) < 0) {
			fprintf(stderr, "%s: Can't remove %s: %s\n",
				prog_name, tabfile, strerror(errno));
			exit(1);
		}
		seteuid(uid);
		printf("Crontab of %s removed\n", user);
		pflag= 1;
	}

	/* Initialize current Time */
	time(&now);

	if (cflag) {
		int fd1, fd2;

		if ((fd1= opentab(uid, file, 'r')) < 0) {
			fprintf(stderr, "%s: Can't open %s: %s\n",
				prog_name, file, strerror(errno));
			exit(1);
		}

		/* Try to parse the new crontab file.  If the parsing
		 * succeeds then 'crontabs' will be non-null.
		 */
		tab_parse(file, user);
		tab_purge();
		if (crontabs == nil) exit(1);

		if ((fd2= opentab(0, tabfile, 'w')) < 0) {
			fprintf(stderr, "%s: Can't open %s: %s\n",
				prog_name, tabfile, strerror(errno));
			exit(1);
		}
		copytab(fd1, file, fd2, tabfile);
		close(fd1);
		close(fd2);
		printf("New crontab for %s installed\n", user);
		pflag= 1;
	}

	if (pflag) {
		/* Alert cron to the new situation. */
		FILE *fp;

		seteuid(0);
		if ((fp= fopen(PIDFILE, "r")) != NULL) {
			unsigned long pid;
			int c;

			pid= 0;
			while ((c= fgetc(fp)) != EOF && c != '\n') {
				if ((unsigned) (c - '0') >= 10) {
					pid= 0; break;
				}
				pid= 10*pid + (c - '0');
				if (pid >= 30000) { pid= 0; break; }
			}
			if (pid > 1 && kill((pid_t) pid, SIGHUP) == 0) {
				pflag= 0;
			}
		}
		seteuid(uid);
		if (pflag) {
			fprintf(stderr,
			"%s: Alerting cron has failed; cron still running?\n",
				prog_name);
			exit(1);
		}
		printf("Cron signalled to reload tables\n");
	}
	return 0;
}

/*
 * $PchId: crontab.c,v 1.4 2000/07/17 18:54:50 philip Exp $
 */
