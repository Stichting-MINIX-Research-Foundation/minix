/* ci - check in 			Author: Peter S. Housel 12/17/87 */

#include <sys/types.h>
#include <string.h>
#include <sys/stat.h>
#include <pwd.h>
#include <signal.h>
#include <stdlib.h>
#include <unistd.h>
#include <time.h>
#include <fcntl.h>
#include <sys/wait.h>
#include <stdio.h>

#define SUFFIX		",S"	/* svc indicator */
#define SVCDIR		"SVC"	/* svc postfix indicator */

#define LINELEN		256	/* maximum line length */

#ifndef PATCH
#define FIX		"fix $1 Fix.$1 > New.$1; mv New.$1 $1\n"
#else
#define FIX		"patch -n -s $1 < Fix.$1; rm -f $1.orig\n"
#endif /* !PATCH */

#ifdef MAXPATHLEN
#define PATHLEN MAXPATHLEN
#else
#define PATHLEN 128		/* buffer length for filenames */
#endif

int unlocked = 0;		/* leave unlocked after checkin */
int relock = 0;			/* lock next revision after checkin */
char file[PATHLEN];		/* file to be checked in */
char svc[PATHLEN];		/* filename for svc file */
char newsvc[PATHLEN];		/* new copy of SVC file */
char line[LINELEN];		/* temporary line buffer */
char *p;			/* scratch character pointer */

FILE *svcfp;			/* svc file */
FILE *origfp, *newfp;		/* "orig" and "new" temp files */
FILE *srcfp;			/* source file */
int rev;			/* new revision number */
int status;			/* wait() buffer */
struct stat stb1, stb2;		/* stat buffers for size compare */
char original[] = "/tmp/cioXXXXXX";	/* previous revision */
char diffout[] = "/tmp/cidXXXXXX";	/* diffs */

_PROTOTYPE(int main, (int argc, char **argv));
_PROTOTYPE(void rundiff, (void));
_PROTOTYPE(void logmsg, (FILE *fp));
_PROTOTYPE(void fname, (char *src, char *dst));
_PROTOTYPE(void svcname, (char *src, char *dst));
_PROTOTYPE(int lockcheck, (FILE *fp, int rev));
_PROTOTYPE(void onintr, (int dummy));
_PROTOTYPE(void clean, (void));
_PROTOTYPE(char *whoami, (void));

int main(argc, argv)
int argc;
char **argv;
{
#ifdef perprintf
  char errbuf[BUFSIZ];
  setbuf(stderr, errbuf);
  perprintf(stderr);
#endif

  while (++argv, --argc) {
	if ('-' == (*argv)[0]) {
		if ('u' == (*argv)[1])
			++unlocked;
		else if ('l' == (*argv)[1])
			++relock;
		else {
			fprintf(stderr, "ci: illegal option -%c\n", (*argv)[1]);
			exit(1);
		}
	} else
		break;
  }

  if (1 != argc) {
	fprintf(stderr, "ci: bad number of files arguments\n");
	exit(1);
  }
  fname(*argv, file);
  svcname(file, svc);

  fprintf(stderr, "%s -> %s\n", file, svc);

  signal(SIGHUP, onintr);
  signal(SIGINT, onintr);
  signal(SIGTERM, onintr);

#ifndef BSD
  if (NULL == (p = strrchr(file, '/')))
	p = file;
  else
	++p;

  if (strlen(p) > 13) {
	fprintf(stderr, "ci: filename %s is too long\n", p);
	exit(1);
  }
#endif /* !BSD */

  strcpy(newsvc, svc);
  *(strrchr(newsvc, ',')) = ';';	/* temporary file will be "file;S" */

  if (NULL == (newfp = fopen(newsvc, "w"))) {
	perror("ci: can't create SVC temporary");
	exit(1);
  }
  (void) mktemp(original);
  (void) mktemp(diffout);

  if (NULL != (svcfp = fopen(svc, "r"))) {	/* does svc-file exist? */
	fgets(line, LINELEN, svcfp);
	if (1 != sscanf(line, "# %d", &rev)) {
		fprintf(stderr, "ci: %s: illegal SVC file header\n", svc);
		exit(1);
	}
	++rev;

	if (!lockcheck(svcfp, rev)) {
		fprintf(stderr, "Revision %d not locked\n", rev);
		clean();
		exit(1);
	}
	if (NULL == (origfp = fopen(original, "w"))) {
		fprintf(stderr, "ci: can't create %s", original);
		perror(" ");
	}
	fgets(line, LINELEN, svcfp);	/* skip "cat <<***MAIN-eof***" line */

	while (NULL != fgets(line, LINELEN, svcfp)
	       && strcmp(line, "***MAIN-eof***\n")) {
		fputs(line, origfp);
		if (ferror(origfp)) {
			perror("ci: origfile");
			exit(1);
		}
	}
	fclose(origfp);

	rundiff();

	if (0 != stat(original, &stb1) || 0 != stat(diffout, &stb2)) {
		perror("ci: can't stat original or diffout");
		clean();
		exit(1);
	}
  } else {			/* no - create one */
	rev = 1;
  }

  fprintf(newfp, "# %d\n", rev);
  fprintf(newfp, "cat <<***MAIN-eof*** >$1\n");
  if (NULL == (srcfp = fopen(file, "r"))) {
	perror("ci: can't read source file");
	clean();
	exit(1);
  }
  while (NULL != fgets(line, LINELEN, srcfp)) fputs(line, newfp);
  fclose(srcfp);
  fputs("***MAIN-eof***\n", newfp);

  if (rev > 1) {
	fprintf(newfp, "if test $2 -ge %d ; then rm -f Fix.$1 ; exit 0 ; fi ; cat <<***%d-eof*** >Fix.$1\n", rev, rev);
	p = (stb1.st_size <= stb2.st_size) ? original : diffout;
	if (NULL == (origfp = fopen(p, "r"))) {
		perror("can't open diff output file");
		clean();
		exit(1);
	}
	while (NULL != fgets(line, LINELEN, origfp)) fputs(line, newfp);
	fclose(origfp);
	fprintf(newfp, "***%d-eof***\n", rev);
	fputs((original == p) ? "mv Fix.$1 $1\n" : FIX, newfp);
	logmsg(newfp);
	while (NULL != fgets(line, LINELEN, svcfp) && strncmp(line, "#***SVCLOCK***", (size_t)14))
		fputs(line, newfp);
  } else {
	logmsg(newfp);
	fputs("rm -f Fix.$1\n", newfp);
  }

  if (relock) {
	fprintf(stderr, "(relocking into revision %d)\n", rev + 1);
	fprintf(newfp, "#***SVCLOCK*** %s %d\n", whoami(), rev + 1);
  }
  signal(SIGHUP, SIG_IGN);	/* disable during critical section */
  signal(SIGINT, SIG_IGN);

  if (ferror(newfp) || fclose(newfp) || ((rev > 1) && unlink(svc))
      || link(newsvc, svc)) {
	fprintf(stderr, "SVC file write/link error - Checkin aborted\n");
	clean();
	exit(1);
  } else
	fprintf(stderr, "Checkin complete.\n");

  if (stat(svc, &stb1) < 0 || chmod(svc, stb1.st_mode & 0555) < 0)
	perror("ci: can't chmod SVC file");

  if (unlocked) {
	if (stat(file, &stb1) < 0 || chmod(file, stb1.st_mode & 0555) < 0)
		perror("ci: can't chmod source file");
  } else if (relock) {
	if (stat(file, &stb1) < 0 || chmod(file, stb1.st_mode | 0200) < 0)
		perror("ci: can't chmod source file");
  } else
	unlink(file);

  clean();
  return(0);
}

void rundiff()
{				/* do "diff file original > diffout" */
  int fd;			/* redirected output file */

  switch (fork()) {
      case -1:
	perror("ci: fork");	/* error */
	clean();
	exit(1);

      case 0:			/* child */
	if ((fd = creat(diffout, 0600)) < 0 || -1 == dup2(fd, 1)) {
		perror("ci: diffout");
		clean();
		exit(1);
	}
	close(fd);
	execlp("diff", "diff", file, original, (char *) 0);
	perror("ci: exec diff failed");
	exit(1);

      default:	break;		/* parent */
}
  wait(&status);
  if (0 != status && 1 << 8 != status) {
	fprintf(stderr, "ci: bad return status (0x%x) from diff\n", status);
	clean();
	exit(1);
  }
}

void logmsg(fp)
FILE *fp;
{
  long now;

  time(&now);
  fprintf(stderr, "Enter log message for revision %d (end with ^D or '.'):\n", rev);
  fprintf(fp, "#***SVC*** revision %d %s %s", rev, file, ctime(&now));
  while (NULL != gets(line) && strcmp(line, "."))
	fprintf(fp, "#***SVC*** %s\n", line);
}

void fname(src, dst)
char *src, *dst;
{
  char *p;
  strcpy(dst, src);
  p = &dst[strlen(src) - strlen(SUFFIX)];
  if (!strcmp(p, SUFFIX)) *p = '\0';
}

void svcname(src, dst)
char *src, *dst;
{
  char *p;

  strcpy(dst, src);
  strcat(dst, SUFFIX);

  if (0 != access(dst, 4)) {
	char dirname[PATHLEN];
	if (NULL != (p = strrchr(src, '/')))
		strncpy(dirname, src, (size_t)(p - src + 1));
	else
		dirname[0] = '\0';
	strcat(dirname, SVCDIR);

	if (0 == access(dirname, 1)) {
		strcpy(dst, dirname);
		if (NULL == p) {
			strcat(dst, "/");
			strcat(dst, src);
		} else
			strcat(dst, p);
		strcat(dst, SUFFIX);
	}
  }
}

int lockcheck(fp, rev)
FILE *fp;
int rev;
{
  char lock[40], check[40];
  long pos;
  int ret;

  sprintf(lock, "#***SVCLOCK*** %s %d\n", whoami(), rev);

  pos = ftell(fp);
  fseek(fp, -((long) strlen(lock)), 2);
  fgets(check, 40, fp);
  ret = (0 == strcmp(lock, check));
  fseek(fp, pos, 0);

  return ret;
}

void onintr(dummy)
int dummy; /* to keep the compiler happy */
{
  fprintf(stderr, "Interrupt - Aborting checkin, cleaning up\n");
  clean();
  exit(1);
}

void clean()
{
  if (strlen(original))		/* if only more programs made this check! */
	unlink(original);
  if (strlen(diffout)) unlink(diffout);
  if (strlen(newsvc)) unlink(newsvc);
}

char *whoami()
{
  struct passwd *pw;

  if (NULL != (pw = getpwuid(getuid())))
	return pw->pw_name;
  else
	return "nobody";
}
