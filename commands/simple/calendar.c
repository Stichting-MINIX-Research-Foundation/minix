/* calendar - reminder service		Authors: S. & K. Hirabayashi */

/* Permission is hereby granted for nonprofit use. */

#include <sys/types.h>
#include <sys/stat.h>
#include <time.h>
#include <regexp.h>
#include <limits.h>
#include <stdlib.h>
#include <string.h>
#include <termcap.h>
#include <unistd.h>
#include <utime.h>
#include <stdio.h>

/* Change these two lines for your system needs. */
#define MAIL1	"/usr/bin/mail"
#define MAIL2	"/bin/mail"
#define PASSWD	"/etc/passwd"	/* system password file */
#define MAX_EXP		4	/* see date_exp() function */

char *mail;			/* mail command path ("/bin/mail" etc) */
regexp *exp[MAX_EXP];		/* date expressions */
int nexp;			/* # of the date expressions */
char calfile[PATH_MAX];		/* calendar file for the user */

int rflg;			/* consult aged 'calendar' file and touch */
int mflg;			/* mail (multi user) service */
char *cmd;			/* the name of this command */
char buf[BUFSIZ];

_PROTOTYPE(int main, (int argc, char **argv));
_PROTOTYPE(void calendar, (void));
_PROTOTYPE(char *getstr, (char *s, int n));
_PROTOTYPE(int newaccess, (char *file));
_PROTOTYPE(void grep, (char *file, char *user));
_PROTOTYPE(int date_exp, (void));
_PROTOTYPE(char *date_pat, (time_t t));
_PROTOTYPE(void regerror, (char *s));
_PROTOTYPE(void error, (char *s, char *t));

int main(argc, argv)
int argc;
char **argv;
{
  char *s;

  cmd = *argv;
  while (--argc > 0 && (*++argv)[0] == '-') {
	s = argv[0] + 1;
	if (*s == '\0')
		mflg++;		/* mail service */
	else if (strcmp(s, "r") == 0)
		rflg++, mflg++;
  }

  if (mflg) {			/* check mailing agent */
	if (access(MAIL1, X_OK) == 0)
		mail = MAIL1;
	else if (access(MAIL2, X_OK) == 0)
		mail = MAIL2;
	else
		error("cannot find %s", MAIL1);
  }
  nexp = date_exp();
  calendar();
  exit(0);
}

void calendar()
{
  int i;
  char *s;
  FILE *fp;

  if (!mflg) {
	grep("calendar", "");
	return;
  }

  /* Mail sevice */
  if ((fp = fopen(PASSWD, "r")) == (FILE *) NULL)
	error("cannot open %s", PASSWD);

  while (fgets(buf, BUFSIZ, fp) != (char *) NULL) {
	for (i = 0, s = buf; *s && *s != '\n'; s++)
		if (*s == ':') i++;
	*s = '\0';
	if (i != 6) error("illegal '/etc/passwd' format: %s", buf);

	/* Calendar file = ${HOME}/calendar */
	sprintf(calfile, "%s/%s", getstr(buf, 5), "calendar");

	if ((access(calfile, R_OK) != 0) || (rflg && !newaccess(calfile)))
		continue;

	grep(calfile, getstr(buf, 0));
  }

  fclose(fp);
}

char *getstr(s, n)
char *s;
int n;
{
/* Returns the string value of the n-th field in the record (s) */
  int i;
  char *t;
  static char str[512];

  for (i = 0; i < n && *s; s++)
	if (*s == ':') i++;		/* field separator */
  for (i = 0, t = str; *s && *s != ':' && i < 511; i++) *t++ = *s++;
  *t = '\0';
  return str;
}

int newaccess(file)
char *file;			/* file name */
{
/* Check whether the file has been touched today. */

  int r = 0;
  struct tm *tm;
  struct stat stbuf;
  time_t clk;
  char newdate[8], olddate[8];

  time(&clk);
  tm = localtime(&clk);
  sprintf(newdate, "%02d%02d%02d", tm->tm_year, tm->tm_mon + 1, tm->tm_mday);

  if (stat(file, &stbuf) == -1) error("cannot stat %s", file);
  tm = localtime(&stbuf.st_mtime);
  sprintf(olddate, "%02d%02d%02d", tm->tm_year, tm->tm_mon + 1, tm->tm_mday);

  if (strcmp(newdate, olddate) != 0) {
	utime(file, NULL);	/* touch */
	r++;
  }
  return r;
}

void grep(file, user)
char *file, *user;
{				/* grep 'exp[]' [| mail user] */
  int i;
  char command[128];		/* mail command */
  FILE *ifp, *ofp;

  if ((ifp = fopen(file, "r")) == (FILE *) NULL)
	error("cannot open %s", file);
  if (*user != '\0') {
	sprintf(command, "%s %s", mail, user);
	ofp = (FILE *) NULL;
  } else {
	ofp = stdout;
  }

  while (fgets(buf, BUFSIZ, ifp) != (char *) NULL) {
	for (i = 0; i < nexp; i++) {
		if (regexec(exp[i], buf, 1)) {
			if ((ofp == (FILE *) NULL) &&
				  (ofp = popen(command, "w")) == (FILE *) NULL)
				error("cannot popen %s", mail);
			fputs(buf, ofp);
			break;
		}
	}
  }

  fclose(ifp);
  if (ofp == stdout)
	fflush(ofp);
  else if (ofp != (FILE *) NULL)
	pclose(ofp);
}

int date_exp()
{
/* Set compiled regular expressions into the exp[] array. */
  static int n[] = {2, 2, 2, 2, 2, 4, 3};
  int i, r, wday;
  time_t clk;

  time(&clk);
  wday = localtime(&clk)->tm_wday;
  r = n[wday];
  if (r > MAX_EXP) error("too many date expressions", "");
  for (i = 0; i < r; i++) {
	exp[i] = regcomp(date_pat(clk));
	clk += 60 * 60 * 24L;	/* 24 hours */
  }
  return(r);
}

char *date_pat(t)
time_t t;
{				/* returns date expression for the time (t) */
  static char *month[] = {
	 "[Jj]an", "[Ff]eb", "[Mm]ar", "[Aa]pr", "[Mm]ay", "[Jj]un",
	  "[Jj]ul", "[Aa]ug", "[Ss]ep", "[Oo]ct", "[Nn]ov", "[Dd]ec"
  };
  static char str[512];
  struct tm *tm;

  tm = localtime(&t);
  sprintf(str,
	"(^|[ \t(,;])(((%s[^ \t]*[ \t])|0*%d/|\\*/)(0*%d|\\*))([^0123456789]|$)",
	month[tm->tm_mon], tm->tm_mon + 1, tm->tm_mday);

  return str;
}

void regerror(s)
char *s;
{				/* regcomp() needs this */
  error("REGULAR EXPRESSION ERROR (%s)", s);
}

void error(s, t)
char *s, *t;
{
  fprintf(stderr, "%s: ", cmd);
  fprintf(stderr, s, t);
  fprintf(stderr, "\n");
  exit(1);
}
