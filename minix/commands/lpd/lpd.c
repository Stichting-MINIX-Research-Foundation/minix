/*	lpd 1.6 - Printer daemon			Author: Kees J. Bot
 *								3 Dec 1989
 */
#define nil 0
#include <stdio.h>
#include <stdlib.h>
#include <limits.h>
#include <string.h>
#include <errno.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <dirent.h>
#include <fcntl.h>
#include <unistd.h>
#include <termcap.h>

char PRINTER[] =	"/dev/lp";
char SPOOL[] =		"/usr/spool/lpd";
char LOG[] =		"/dev/log";

void report(char *mess)
{
	fprintf(stderr, "lpd: %s: %s\n", mess, strerror(errno));
}

void fatal(char *mess)
{
	report(mess);
	exit(1);
}

char jobX[] = "jobXXXXXX";
char tmpX[] = "tmpXXXXXX";

void spoolerr(char *file)
{
	unlink(jobX);
	unlink(tmpX);
	fatal(file);
}

void spool(char *path)
/* Place a file into the spool directory, either by copying it, or by leaving
 * a reference.
 */
{
	char *file;
	int j, u;

	mktemp(jobX);
	file= mktemp(tmpX);

	if (path[0] == '/') {
		int f;

		if ((f= open(path, O_RDONLY)) >= 0) {
			close(f);
			file= path;
		}
	}
	if (file != path) {
		int c;
		FILE *t;

		if ((t= fopen(tmpX, "w")) == nil) spoolerr(tmpX);

		while ((c= getchar()) != EOF && putc(c, t) != EOF) {}

		if (ferror(stdin)) spoolerr(path);

		if (ferror(t) || fclose(t) == EOF) spoolerr(tmpX);

		fclose(stdin);
	}

	if ((j= open(jobX, O_WRONLY|O_CREAT|O_EXCL, 0000)) < 0) spoolerr(jobX);

	u= getuid();
	if (write(j, file, strlen(file)+1) < 0
		|| write(j, &u, sizeof(u)) < 0
		|| write(j, path, strlen(path)+1) < 0
		|| close(j) < 0
		|| chmod(jobX, 0600) < 0
	) spoolerr(jobX);
}

struct job {
	struct job *next;
	time_t age;
	char name[sizeof(jobX)];
} *jobs = nil;

int job(void)
/* Look for print jobs in the spool directory.  Make a list of them sorted
 * by age.  Return true iff the list is nonempty.
 */
{
	DIR *spool;
	struct dirent *entry;
	struct job *newjob, **ajob;
	struct stat st;

	if (jobs != nil) return 1;

	if ((spool= opendir(".")) == nil) fatal(SPOOL);

	while ((entry= readdir(spool)) != nil) {
		if (strncmp(entry->d_name, "job", 3) != 0) continue;

		if (stat(entry->d_name, &st) < 0
			|| (st.st_mode & 0777) == 0000) continue;

		if ((newjob= malloc(sizeof(*newjob))) == nil) fatal("malloc()");
		newjob->age = st.st_mtime;
		strcpy(newjob->name, entry->d_name);

		ajob= &jobs;
		while (*ajob != nil && (*ajob)->age < newjob->age)
			ajob= &(*ajob)->next;

		newjob->next= *ajob;
		*ajob= newjob;
	}
	closedir(spool);

	return jobs != nil;
}

/* What to do with control-X:
 * 0 ignore,
 * 1 give up on controlling the printer, assume user knows how printer works,
 * 2 print.
 */
char control[] = {
	0, 1, 1, 1, 1, 1, 1, 0,		/* \0, \a  don't show.	*/
	2, 2, 2, 1, 2, 2, 1, 1,		/* \b, \t, \n, \f, \r	*/
	1, 1, 1, 1, 1, 1, 1, 1,
	1, 1, 1, 1, 1, 1, 1, 1
};

int lp;
char buf[BUFSIZ];
int count, column, line, ncols = 80, nlines = 66;

void flush(void)
/* Copy the characters in the output buffer to the printer, with retries if
 * out of paper.
 */
{
	char *bp= buf;

	while (count > 0) {
		int retry = 0, complain = 0;
		int r;

		while ((r= write(lp, bp, count)) < 0) {
			if (errno != EAGAIN) fatal(PRINTER);
			if (retry == complain) {
				fprintf(stderr,
					"lpd: %s: Printer out of paper\n",
					PRINTER);
				complain= retry + 60;
			}
			sleep(1);
			retry++;
		}
		bp+= r;
		count-= r;
	}
	count = 0;
}

void put(int c)
/* Send characters to the output buffer to be printed and do so if the buffer
 * is full.  Track the position of the write-head in `column' and `line'.
 */
{
	buf[count++] = c;

	switch (c) {
	case '\f':
		column = 0;
		line = 0;
		break;
	case '\r':
		column = 0;
		break;
	case '\n':
		line++;
		break;
	case '\b':
		column--;
		break;
	default:
		if (++column > ncols) { line++; column= 1; }
	}
	if (line == nlines) line= 0;

	if (count == BUFSIZ) flush();
}

void print(FILE *f)
/* Send the contents of an open file to the printer.  Expand tabs and change
 * linefeed to a carriage-return linefeed sequence.  Print a formfeed at the
 * end if needed to reach the top of the next page.  If a control character
 * is printed that we do not know about, then the user is assumed to know
 * what they are doing, so output processing is disabled.
 */
{
	int c;

	count= column= line= 0;

	while ((c= getc(f)) != EOF) {
		if (c < ' ') {
			switch (control[c]) {
			case 0:	continue;	/* Ignore this one. */
			case 1:
				/* Can't handle this junk, assume smart user. */
				do {
					buf[count++] = c;
					if (count == BUFSIZ) flush();
				} while ((c= getc(f)) != EOF);

				flush();
				return;
			case 2: /* fine */;
			}
		}

		switch (c) {
		case '\n':
			put('\r');
			put('\n');
			break;
		case '\t':
			do {
				put(' ');
			} while (column & 07);
			break;
		case '\b':
			if (column > 0) put(c);
			break;
		default:
			put(c);
		}
	}
	if (column > 0) { put('\r'); put('\n'); }
	if (line > 0) put('\f');
	flush();
	return;
}

void joberr(char *job)
{
	fprintf(stderr, "lpd: something is wrong with %s\n", job);

	if (unlink(job) < 0) fatal("can't remove it");
}

void work(void)
/* Print all the jobs in the job list. */
{
	FILE *j, *f;
	char file[PATH_MAX+1], *pf=file;
	int c;
	struct job *job;

	job= jobs;
	jobs= jobs->next;

	if ((j= fopen(job->name, "r")) == nil) {
		joberr(job->name);
		return;
	}

	do {
		if (pf == file + sizeof(file) || (c= getc(j)) == EOF) {
			fclose(j);
			joberr(job->name);
			return;
		}
		*pf++ = c;
	} while (c != 0);

	fclose(j);

	if ((f= fopen(file, "r")) == nil)
		fprintf(stderr, "lpd: can't read %s\n", file);
	else {
		print(f);
		fclose(f);
	}
	if (file[0] != '/' && unlink(file) < 0) report(file);

	if (unlink(job->name) < 0) fatal(job->name);
	free(job);
}

void getcap(void)
/* Find the line printer dimensions in the termcap database under "lp". */
{
	char printcap[1024];
	int n;

	if (tgetent(printcap, "lp") == 1) {
		if ((n= tgetnum("co")) > 0) ncols= n;
		if ((n= tgetnum("li")) > 0) nlines= n;
	}
}

void haunt(void)
/* Become a daemon, print jobs while there are any, exit. */
{
	int fd;

	if ((fd= open("/dev/tty", O_RDONLY)) != -1) {
		/* We have a controlling tty!  Disconnect. */
		close(fd);

		switch(fork()) {
		case -1:	fatal("can't fork");
		case  0:	break;
		default:	exit(0);
		}

		if ((fd= open("/dev/null", O_RDONLY)) < 0) fatal("/dev/null");
		dup2(fd, 0);
		close(fd);
		if ((fd= open(LOG, O_WRONLY)) < 0) fatal(LOG);
		dup2(fd, 1);
		dup2(fd, 2);
		close(fd);
		setsid();
	}

	getcap();

	do {
		if ((lp= open(PRINTER, O_WRONLY)) < 0) {
			/* Another lpd? */
			if (errno == EBUSY) exit(0);
			fatal(PRINTER);
		}

		while (job()) work();

		close(lp);
	} while (job());
}

int main(int argc, char **argv)
{
	if (argc > 2) {
		fprintf(stderr, "Usage: %s [path | stdin < path]\n", argv[0]);
		exit(1);
	}

	umask(0077);

	if (chdir(SPOOL) < 0) fatal(SPOOL);

	if (argc == 2) spool(argv[1]);

	haunt();
}
