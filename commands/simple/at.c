/* at - run a command at a specified time	Author: Jan Looyen */

#include <sys/types.h>
#include <time.h>
#include <fcntl.h>
#include <unistd.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <stdio.h>
#include <limits.h>
#include <signal.h>
#include <errno.h>

#define	STARTDAY	0	/* see ctime(3)	 */
#define	LEAPDAY		STARTDAY+59
#define	MAXDAYNR	STARTDAY+365
#define	NODAY		-2
char CRONPID[]	=	"/usr/run/cron.pid";

_PROTOTYPE(int main, (int argc, char **argv, char **envp));
_PROTOTYPE(int getltim, (char *t));
_PROTOTYPE(int getlday, (char *m, char *d));
_PROTOTYPE(int digitstring, (char *s));

int main(argc, argv, envp)
int argc;
char **argv, **envp;
{
  int i, c, mask, ltim, year, lday = NODAY;
  char buf[64], job[30], pastjob[35], *dp, *sp;
  struct tm *p;
  long clk;
  FILE *fp;
  char pwd[PATH_MAX+1];

/*-------------------------------------------------------------------------*
 *	check arguments	& pipe to "pwd"				           *
 *-------------------------------------------------------------------------*/
  if (argc < 2 || argc > 5) {
	fprintf(stderr, "Usage: %s time [month day] [file]\n", argv[0]);
	exit(1);
  }
  if ((ltim = getltim(argv[1])) == -1) {
	fprintf(stderr, "%s: wrong time specification\n", argv[0]);
	exit(1);
  }
  if ((argc == 4 || argc == 5) && (lday = getlday(argv[2], argv[3])) == -1) {
	fprintf(stderr, "%s: wrong date specification\n", argv[0]);
	exit(1);
  }
  if ((argc == 3 || argc == 5) && open(argv[argc - 1], O_RDONLY) == -1) {
	fprintf(stderr, "%s: cannot find: %s\n", argv[0], argv[argc - 1]);
	exit(1);
  }
  if (getcwd(pwd, sizeof(pwd)) == NULL) {
	fprintf(stderr, "%s: cannot determine current directory: %s\n",
		argv[0], strerror(errno));
	exit(1);
  }

/*-------------------------------------------------------------------------*
 *	determine execution time and create 'at' job file		   *
 *-------------------------------------------------------------------------*/
  time(&clk);
  p = localtime(&clk);
  year = p->tm_year;
  if (lday == NODAY) {		/* no [month day] given */
	lday = p->tm_yday;
	if (ltim <= (p->tm_hour * 100 + p->tm_min)) {
		lday++;
		if ((lday == MAXDAYNR && (year % 4)) || lday == MAXDAYNR + 1) {
			lday = STARTDAY;
			year++;
		}
	}
  } else
	switch (year % 4) {
	    case 0:
		if (lday < p->tm_yday ||
		    (lday == p->tm_yday &&
		    ltim <= (p->tm_hour * 100 + p->tm_min))) {
			year++;
			if (lday > LEAPDAY) lday--;
		}
		break;
	    case 1:
	    case 2:
		if (lday > LEAPDAY) lday--;
		if (lday < p->tm_yday ||
		    (lday == p->tm_yday &&
		     ltim <= (p->tm_hour * 100 + p->tm_min)))
			year++;
		break;
	    case 3:
		if (lday < ((lday > LEAPDAY) ? p->tm_yday + 1 : p->tm_yday) ||
		    (lday ==((lday > LEAPDAY) ? p->tm_yday + 1 : p->tm_yday) &&
		     ltim <= (p->tm_hour * 100 + p->tm_min)))
			year++;
		else if (lday > LEAPDAY)
			lday--;
		break;
	}
  sprintf(job, "/usr/spool/at/%02d.%03d.%04d.%02d",
	year % 100, lday, ltim, getpid() % 100);
  sprintf(pastjob, "/usr/spool/at/past/%02d.%03d.%04d.%02d",
	year % 100, lday, ltim, getpid() % 100);
  mask= umask(0077);
  if ((fp = fopen(pastjob, "w")) == NULL) {
	fprintf(stderr, "%s: cannot create %s: %s\n",
		argv[0], pastjob, strerror(errno));
	exit(1);
  }

/*-------------------------------------------------------------------------*
 *	write environment and command(s) to 'at'job file		   *
 *-------------------------------------------------------------------------*/
  i = 0;
  while ((sp= envp[i++]) != NULL) {
	dp = buf;
	while ((c= *sp++) != '\0' && c != '=' && dp < buf+sizeof(buf)-1)
		*dp++ = c;
	if (c != '=') continue;
	*dp = '\0';
	fprintf(fp, "%s='", buf);
	while (*sp != 0) {
		if (*sp == '\'')
			fprintf(fp, "'\\''");
		else
			fputc(*sp, fp);
		sp++;
	}
	fprintf(fp, "'; export %s\n", buf);
  }
  fprintf(fp, "cd '%s'\n", pwd);
  fprintf(fp, "umask %o\n", mask);
  if (argc == 3 || argc == 5)
	fprintf(fp, "%s\n", argv[argc - 1]);
  else				/* read from stdinput */
	while ((c = getchar()) != EOF) putc(c, fp);
  fclose(fp);

  if (chown(pastjob, getuid(), getgid()) == -1) {
	fprintf(stderr, "%s: cannot set ownership of %s: %s\n",
		argv[0], pastjob, strerror(errno));
	unlink(pastjob);
	exit(1);
  }
  /* "Arm" the job. */
  if (rename(pastjob, job) == -1) {
	fprintf(stderr, "%s: cannot move %s to %s: %s\n",
		argv[0], pastjob, job, strerror(errno));
	unlink(pastjob);
	exit(1);
  }
  printf("%s: %s created\n", argv[0], job);

  /* Alert cron to the new situation. */
  if ((fp= fopen(CRONPID, "r")) != NULL) {
	unsigned long pid;

	pid= 0;
	while ((c= fgetc(fp)) != EOF && c != '\n') {
		if ((unsigned) (c - '0') >= 10) { pid= 0; break; }
		pid= 10*pid + (c - '0');
		if (pid >= 30000) { pid= 0; break; }
	}
	if (pid > 1) kill((pid_t) pid, SIGHUP);
  }
  return(0);
}

/*-------------------------------------------------------------------------*
 *	getltim()		return((time OK) ? daytime : -1)	   *
 *-------------------------------------------------------------------------*/
int getltim(t)
char *t;
{
  if (t[4] == '\0' && t[3] >= '0' && t[3] <= '9' &&
      t[2] >= '0' && t[2] <= '5' && t[1] >= '0' && t[1] <= '9' &&
      (t[0] == '0' || t[0] == '1' || (t[1] <= '3' && t[0] == '2')))
	return(atoi(t));
  else
	return(-1);
}

/*-------------------------------------------------------------------------*
 *	getlday()		return ((date OK) ? yearday : -1)	   *
 *-------------------------------------------------------------------------*/
int getlday(m, d)
char *m, *d;
{
  int i, day, im;
  static int cumday[] = {0, 0, 31, 60, 91, 121, 152,
		       182, 213, 244, 274, 305, 335};
  static struct date {
	char *mon;
	int dcnt;
  } *pc, kal[] = {
	{ "Jan", 31 }, { "Feb", 29 }, { "Mar", 31 }, { "Apr", 30 },
	{ "May", 31 }, { "Jun", 30 }, { "Jul", 31 }, { "Aug", 31 },
	{ "Sep", 30 }, { "Oct", 31 }, { "Nov", 30 }, { "Dec", 31 },
  };

  pc = kal;
  im = (digitstring(m)) ? atoi(m) : 0;
  m[0] &= 0337;
  for (i = 1; i < 13 && strcmp(m, pc->mon) && im != i; i++, pc++);
  if (i < 13 && (day = (digitstring(d)) ? atoi(d) : 0) && day <= pc->dcnt) {
	if (!STARTDAY) day--;
	return(day + cumday[i]);
  } else
	return(-1);
}



int digitstring(s)
char *s;
{
  while (*s >= '0' && *s <= '9') s++;
  return((*s == '\0') ? 1 : 0);
}
