/*	lp 1.4 - Send file to the lineprinter		Author: Kees J. Bot
 *								3 Dec 1989
 */
#define nil 0
#include <sys/types.h>
#include <stdio.h>
#include <limits.h>
#include <stdlib.h>
#include <unistd.h>
#include <fcntl.h>
#include <string.h>
#include <errno.h>
#include <sys/wait.h>

char LPD1[] = "/usr/sbin/lpd";		/* Proper place of lpd */
char LPD2[] = "/usr/bin/lpd";		/* Minix has no sbin directories. */

void report(char *mess)
{
	fprintf(stderr, "lp: %s: %s\n", mess, strerror(errno));
}

void fatal(char *mess)
{
	report(mess);
	exit(1);
}

void lp(char *file)
/* Start the lpd daemon giving it the file to spool and print. */
{
	int pid, status;

	if (file[0] != '/' || (pid= fork()) == 0) {
		execl(LPD1, LPD1, file, (char *) nil);
		if (errno != ENOENT) fatal(LPD1);
		execl(LPD2, LPD2, file, (char *) nil);
		fatal(LPD2);
	}

	if (pid < 0) fatal("can't fork");

	if (waitpid(pid, &status, 0) < 0) fatal("wait");

	if (status != 0) exit(1);
}

char path[PATH_MAX+1];
int cwdsize;

int main(int argc, char **argp)
{
	int e=0;
	char *file;

	if (argc <= 1) lp("stdin");

	/* Lpd requires full path names, so find out where we are. */
	if (getcwd(path, sizeof(path)) == nil)
		fatal("Can't determine current directory");

	cwdsize= strlen(path);

	/* Hand each file to lpd. */
	while ((file= *++argp) != nil) {

		close(0);

		if (open(file, O_RDONLY) != 0) {
			report(file);
			e=1;
			continue;
		}
		if (file[0] == '/') {
			lp(file);
			continue;
		}
		if (cwdsize + 1 + strlen(file) + 1 > sizeof(path)) {
			fprintf(stderr,
				"lp: full pathname of %s is too long\n",
				file);
			e=1;
			continue;
		}
		path[cwdsize] = '/';
		strcpy(path + cwdsize + 1, file);

		lp(path);
	}
	exit(e);
}
