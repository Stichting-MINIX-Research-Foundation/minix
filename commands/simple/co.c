/* co - check out			Author: Peter S. Housel 12/24/87 */

#include <sys/types.h>
#include <sys/stat.h>
#include <string.h>
#include <pwd.h>
#include <stdlib.h>
#include <unistd.h>
#include <stdio.h>

#define SUFFIX		",S"	/* svc indicator */
#define SVCDIR		"SVC"	/* svc postfix indicator */

#define LINELEN		256	/* maximum line length */

#ifdef MAXPATHLEN
#define PATHLEN MAXPATHLEN
#else
#define PATHLEN 128		/* buffer length for filenames */
#endif

char file[PATHLEN];		/* file to be checked in */
char svc[PATHLEN];		/* filename for svc file */
char newsvc[PATHLEN];		/* new copy of SVC file */
char line[LINELEN];		/* temporary line buffer */
char *p;			/* scratch character pointer */

FILE *svcfp;			/* svc file */
int rev;			/* old revision number */
int lastrev, lockrev;		/* latest file revision, lock into */
int status;			/* wait() buffer */
int svclock;			/* lock the SVC file */
struct stat stb;		/* stat() buffer */
char *base;			/* basename of file */

char difftemp[PATHLEN];		/* extract() fix/patch input */

_PROTOTYPE(int main, (int argc, char **argv));
_PROTOTYPE(void fname, (char *src, char *dst));
_PROTOTYPE(void svcname, (char *src, char *dst));
_PROTOTYPE(void extract, (char *script, char *out, int rev));
_PROTOTYPE(char *basename, (char *name));
_PROTOTYPE(char *whoami, (void));
_PROTOTYPE(int getyn, (void));

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
		if ('r' == (*argv)[1]) {
			--argc;
			rev = atoi(*++argv);
			if (rev < 1) {
				fprintf(stderr, "Illegal revision number\n");
				exit(1);
			}
		} else if ('l' == (*argv)[1])
			++svclock;
		else {
			fprintf(stderr, "co: illegal option -%c\n", (*argv)[1]);
			exit(1);
		}
	} else
		break;
  }

  if (1 != argc) {
	fprintf(stderr, "co: bad number of files arguments\n");
	exit(1);
  }
  fname(*argv, file);
  svcname(file, svc);

  fprintf(stderr, "%s -> %s\n", svc, base = basename(file));

  if (NULL == (svcfp = fopen(svc, "r"))) {
	perror("co: can't read SVC file");
	exit(1);
  }
  if (1 != fscanf(svcfp, "# %d", &lastrev) || lastrev < 1) {
	fprintf(stderr, "co: illegal SVC file format\n");
	exit(1);
  }
  fclose(svcfp);

  if (stat(base, &stb) >= 0 && (stb.st_mode & 0222)) {
	fprintf(stderr, "Writable %s exists - overwrite (n/y)? ", base);
	if (!getyn()) {
		fprintf(stderr, "Checkout aborted\n");
		exit(1);
	}
  }
  if (strlen(base)) unlink(base);
  if (0 == rev) rev = lastrev;
  fprintf(stderr, "Checking out revision %d", rev);
  extract(svc, base, rev);

  if (svclock) {
	lockrev = lastrev + 1;
	fprintf(stderr, "; Locking into revision %d\n", lockrev);
	if (stat(svc, &stb) < 0 || chmod(svc, stb.st_mode | 0200) < 0)
		perror("co: can't chmod SVC file");

	if (stat(base, &stb) < 0 || chmod(base, stb.st_mode | 0200) < 0)
		perror("co: can't chmod source file");

	if (NULL == (svcfp = fopen(svc, "a"))
	    || (fprintf(svcfp, "#***SVCLOCK*** %s %d\n", whoami(), lockrev), ferror(svcfp))) {
		fprintf(stderr, "co: can't lock %s\n", svc);
		exit(1);
	}
	if (stat(svc, &stb) < 0 || chmod(svc, stb.st_mode & 0555))
		perror("co: can't chmod SVC file");
  } else {
	putchar('\n');
	if (stat(base, &stb) < 0 || chmod(base, stb.st_mode & 0555))
		perror("co: can't chmod source file");
  }

  return(0);
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
		strncpy(dirname, src, (size_t)(p - src) + 1);
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

void extract(script, out, rev)
char *script, *out;
int rev;
{
  FILE *outfp;
  int testrev;
  char buf[80];

  sprintf(difftemp, "Fix.%s", out);

  svcfp = fopen(script, "r");
  fgets(line, LINELEN, svcfp);	/* skip '# rev' line */
  fgets(line, LINELEN, svcfp);	/* skip 'cat <***MAIN-eof***' line */

  if (NULL == (outfp = fopen(out, "w"))) {
	perror("co: can't create output file");
	return;
  }
  while (NULL != fgets(line, LINELEN, svcfp) &&
	  strcmp(line, "***MAIN-eof***\n"))
	fputs(line, outfp);

  fclose(outfp);

  while (NULL != fgets(line, LINELEN, svcfp)) {
	if (!strncmp(line, "if ", (size_t)3)) {
		sscanf(line, "if test $2 -ge %d", &testrev);
		if (rev >= testrev) {
			unlink(difftemp);
			return;
		}
		if (NULL == (outfp = fopen(difftemp, "w"))) {
			perror("co: can't create output file");
			return;
		}
		sprintf(buf, "***%d-eof***\n", testrev);
		while (NULL != fgets(line, LINELEN, svcfp) &&
							strcmp(line, buf))
			fputs(line, outfp);
		fclose(outfp);
	} else if (!strncmp(line, "mv ", (size_t)3)) {
		sprintf(buf, "mv Fix.%s %s", out, out);
		system(buf);
	} else if (!strncmp(line, "fix ", (size_t)4)) {
		sprintf(buf, "fix %s Fix.%s > New.%s; mv New.%s %s", out, out, out, out, out);
		system(buf);
	} else if (!strncmp(line, "patch ", (size_t)6)) {
		sprintf(buf, "patch -n -s %s < Fix.%s; rm -f %s.orig", out, out, out);
		system(buf);
	} else {		/* ignore */
	}
  }

  unlink(difftemp);
  return;
}

char *basename(name)
char *name;
{
  char *p;

  if (NULL == (p = strrchr(name, '/')))
	return name;
  else
	return p + 1;
}

char *whoami()
{
  struct passwd *pw;

  if (NULL != (pw = getpwuid(getuid())))
	return pw->pw_name;
  else
	return "nobody";
}

int getyn()
{
  char ans[10];

  return(NULL != fgets(ans, 10, stdin)) && ('y' == ans[0] || 'Y' == ans[0]);
}
