/*
 * Copyright (c) 1980 Regents of the University of California.
 * All rights reserved.  The Berkeley software License Agreement
 * specifies the terms and conditions for redistribution.
 */

#ifndef lint
static char sccsid[] = "@(#)finger.c 1.1 87/12/21 SMI"; /* from 5.8 3/13/86 */
#endif /* not lint */

/*
 * This is a finger program.  It prints out useful information about users
 * by digging it up from various system files.
 *
 * There are three output formats, all of which give login name, teletype
 * line number, and login time.  The short output format is reminiscent
 * of finger on ITS, and gives one line of information per user containing
 * in addition to the minimum basic requirements (MBR), the full name of
 * the user, his idle time and location.  The
 * quick style output is UNIX who-like, giving only name, teletype and
 * login time.  Finally, the long style output give the same information
 * as the short (in more legible format), the home directory and shell
 * of the user, and, if it exits, a copy of the file .plan in the users
 * home directory.  Finger may be called with or without a list of people
 * to finger -- if no list is given, all the people currently logged in
 * are fingered.
 *
 * The program is validly called by one of the following:
 *
 *	finger			{short form list of users}
 *	finger -l		{long form list of users}
 *	finger -b		{briefer long form list of users}
 *	finger -q		{quick list of users}
 *	finger -i		{quick list of users with idle times}
 *	finger namelist		{long format list of specified users}
 *	finger -s namelist	{short format list of specified users}
 *	finger -w namelist	{narrow short format list of specified users}
 *
 * where 'namelist' is a list of users login names.
 * The other options can all be given after one '-', or each can have its
 * own '-'.  The -f option disables the printing of headers for short and
 * quick outputs.  The -b option briefens long format outputs.  The -p
 * option turns off plans for long format outputs.
 */

#include <sys/types.h>
#include <ctype.h>
#include <errno.h>
#include <fcntl.h>
#include <pwd.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <unistd.h>
#include <utmp.h>
#include <sys/ioctl.h>
#include <sys/stat.h>
#include <net/gen/in.h>
#include <net/gen/inet.h>
#include <net/gen/netdb.h>
#include <net/gen/socket.h>
#include <net/gen/tcp.h>
#include <net/gen/tcp_hdr.h>
#include <net/gen/tcp_io.h>
#include <net/hton.h>
#include <net/netlib.h>

#define NONOTHING	1		/* don't say "No plan", or "No mail" */

#define NONET	0

#define ASTERISK	'*'		/* ignore this in real name */
#define COMMA		','		/* separator in pw_gecos field */
#define COMMAND		'-'		/* command line flag char */
#define SAMENAME	'&'		/* repeat login name in real name */
#define TALKABLE	0220		/* tty is writable if this mode */

struct utmp user;
#define NMAX sizeof(user.ut_name)
#define LMAX sizeof(user.ut_line)
#define HMAX sizeof(user.ut_host)

struct person {			/* one for each person fingered */
	char *name;			/* name */
	char tty[LMAX+1];		/* null terminated tty line */
	char host[HMAX+1];		/* null terminated remote host name */
	long loginat;			/* time of (last) login */
	long idletime;			/* how long idle (if logged in) */
	char *realname;			/* pointer to full name */
	struct passwd *pwd;		/* structure of /etc/passwd stuff */
	char loggedin;			/* person is logged in */
	char writable;			/* tty is writable */
	char original;			/* this is not a duplicate entry */
	struct person *link;		/* link to next person */
	char *where;			/* terminal location */
	char hostt[HMAX+1];		/* login host */
};

#include <paths.h>

char LASTLOG[] = _PATH_LASTLOG;	/* last login info */
char USERLOG[] = _PATH_UTMP;		/* who is logged in */
char PLAN[] = "/.plan";			/* what plan file is */
char PROJ[] = "/.project";		/* what project file */
	
int unbrief = 1;			/* -b option default */
int header = 1;				/* -f option default */
int hack = 1;				/* -h option default */
int idle = 0;				/* -i option default */
int large = 0;				/* -l option default */
int match = 1;				/* -m option default */
int plan = 1;				/* -p option default */
int unquick = 1;			/* -q option default */
int small = 0;				/* -s option default */
int wide = 1;				/* -w option default */

int unshort;
int lf;					/* LASTLOG file descriptor */
struct person *person1;			/* list of people */
long tloc;				/* current time */

#if !defined(__minix)
char *strcpy();
char *ctime();
#endif

char *prog_name;

/* Already defined in stdio.h */
#undef fwopen
#define fwopen finger_fwopen

int main (int argc, char *argv[]);
static void doall(void);
static void donames(char **args);
static void print(void);
static void fwopen(void);
static void decode(struct person *pers);
static void fwclose(void);
static int netfinger (char *name);
static int matchcmp (char *gname, char *login, char *given);
static void quickprint (struct person *pers);
static void shortprint (struct person *pers);
static void personprint (struct person *pers);
static int AlreadyPrinted(int uid);
static int AnyMail (char *name);
static struct passwd *pwdcopy(struct passwd *pfrom);
static void findidle (struct person *pers);
static int ltimeprint (char *dt, long *before, char *after);
static void stimeprint (long *dt);
static void findwhen (struct person *pers);
static int namecmp (char *name1, char *name2);

main(argc, argv)
	int argc;
	register char **argv;
{
	register char *s;

	prog_name= argv[0];

	/* parse command line for (optional) arguments */
	while (*++argv && **argv == COMMAND)
		for (s = *argv + 1; *s; s++)
			switch (*s) {
			case 'b':
				unbrief = 0;
				break;
			case 'f':
				header = 0;
				break;
			case 'h':
				hack = 0;
				break;
			case 'i':
				idle = 1;
				unquick = 0;
				break;
			case 'l':
				large = 1;
				break;
			case 'm':
				match = 0;
				break;
			case 'p':
				plan = 0;
				break;
			case 'q':
				unquick = 0;
				break;
			case 's':
				small = 1;
				break;
			case 'w':
				wide = 0;
				break;
			default:
				fprintf(stderr, "Usage: finger [-bfhilmpqsw] [login1 [login2 ...] ]\n");
				exit(1);
			}
	if (unquick || idle)
		time(&tloc);
	/*
	 * *argv == 0 means no names given
	 */
	if (*argv == 0)
		doall();
	else
		donames(argv);
	if (person1)
		print();
	exit(0);
}

static void doall()
{
	register struct person *p;
	register struct passwd *pw;
	int uf;
	char name[NMAX + 1];

	unshort = large;
	if ((uf = open(USERLOG, 0)) < 0) {
		fprintf(stderr, "finger: error opening %s\n", USERLOG);
		exit(2);
	}
	if (unquick) {
		setpwent();
		fwopen();
	}
	while (read(uf, (char *)&user, sizeof user) == sizeof user) {
		if (user.ut_name[0] == 0)
			continue;
		if (person1 == 0)
			p = person1 = (struct person *) malloc(sizeof *p);
		else {
			p->link = (struct person *) malloc(sizeof *p);
			p = p->link;
		}
		bcopy(user.ut_name, name, NMAX);
		name[NMAX] = 0;
		bcopy(user.ut_line, p->tty, LMAX);
		p->tty[LMAX] = 0;
		bcopy(user.ut_host, p->host, HMAX);
		p->host[HMAX] = 0;
		p->loginat = user.ut_time;
		p->pwd = 0;
		p->loggedin = 1;
		p->where = NULL;
		if (unquick && (pw = getpwnam(name))) {
			p->pwd = pwdcopy(pw);
			decode(p);
			p->name = p->pwd->pw_name;
		} else
			p->name = strcpy(malloc(strlen(name) + 1), name);
	}
	if (unquick) {
		fwclose();
		endpwent();
	}
	close(uf);
	if (person1 == 0) {
		printf("No one logged on\n");
		return;
	}
	p->link = 0;
}

static void donames(argv)
	char **argv;
{
	register struct person *p;
	register struct passwd *pw;
	int uf;

	/*
	 * get names from command line and check to see if they're
	 * logged in
	 */
	unshort = !small;
	for (; *argv != 0; argv++) {
		if (netfinger(*argv))
			continue;
		if (person1 == 0)
			p = person1 = (struct person *) malloc(sizeof *p);
		else {
			p->link = (struct person *) malloc(sizeof *p);
			p = p->link;
		}
		p->name = *argv;
		p->loggedin = 0;
		p->original = 1;
		p->pwd = 0;
	}
	if (person1 == 0)
		return;
	p->link = 0;
	/*
	 * if we are doing it, read /etc/passwd for the useful info
	 */
	if (unquick) {
		setpwent();
		if (!match) {
			for (p = person1; p != 0; p = p->link)
				if (pw = getpwnam(p->name))
					p->pwd = pwdcopy(pw);
		} else while ((pw = getpwent()) != 0) {
			for (p = person1; p != 0; p = p->link) {
				if (!p->original)
					continue;
				if (strcmp(p->name, pw->pw_name) != 0 &&
				    !matchcmp(pw->pw_gecos, pw->pw_name, p->name))
					continue;
				if (p->pwd == 0)
					p->pwd = pwdcopy(pw);
				else {
					struct person *new;
					/*
					 * handle multiple login names, insert
					 * new "duplicate" entry behind
					 */
					new = (struct person *)
						malloc(sizeof *new);
					new->pwd = pwdcopy(pw);
					new->name = p->name;
					new->original = 1;
					new->loggedin = 0;
					new->link = p->link;
					p->original = 0;
					p->link = new;
					p = new;
				}
			}
		}
		endpwent();
	}
	/* Now get login information */
	if ((uf = open(USERLOG, 0)) < 0) {
		fprintf(stderr, "finger: error opening %s\n", USERLOG);
		exit(2);
	}
	while (read(uf, (char *)&user, sizeof user) == sizeof user) {
		if (*user.ut_name == 0)
			continue;
		for (p = person1; p != 0; p = p->link) {
			if (p->loggedin == 2)
				continue;
			if (strncmp(p->pwd ? p->pwd->pw_name : p->name,
				    user.ut_name, NMAX) != 0)
				continue;
			if (p->loggedin == 0) {
				bcopy(user.ut_line, p->tty, LMAX);
				p->tty[LMAX] = 0;
				bcopy(user.ut_host, p->host, HMAX);
				p->host[HMAX] = 0;
				p->loginat = user.ut_time;
				p->loggedin = 1;
			} else {	/* p->loggedin == 1 */
				struct person *new;
				new = (struct person *) malloc(sizeof *new);
				new->name = p->name;
				bcopy(user.ut_line, new->tty, LMAX);
				new->tty[LMAX] = 0;
				bcopy(user.ut_host, new->host, HMAX);
				new->host[HMAX] = 0;
				new->loginat = user.ut_time;
				new->pwd = p->pwd;
				new->loggedin = 1;
				new->original = 0;
				new->link = p->link;
				p->loggedin = 2;
				p->link = new;
				p = new;
			}
		}
	}
	close(uf);
	if (unquick) {
		fwopen();
		for (p = person1; p != 0; p = p->link)
			decode(p);
		fwclose();
	}
}

static void print()
{
	register FILE *fp;
	register struct person *p;
	register char *s;
	register c;

	/*
	 * print out what we got
	 */
	if (header) {
		if (unquick) {
			if (!unshort)
				if (wide)
					printf("Login       Name              TTY Idle    When            Where\n");
				else
					printf("Login    TTY Idle    When            Where\n");
		} else {
			printf("Login      TTY            When");
			if (idle)
				printf("             Idle");
			putchar('\n');
		}
	}
	for (p = person1; p != 0; p = p->link) {
		if (!unquick) {
			quickprint(p);
			continue;
		}
		if (!unshort) {
			shortprint(p);
			continue;
		}
		personprint(p);
		if (p->pwd != 0 && !AlreadyPrinted(p->pwd->pw_uid)) {
			AnyMail(p->pwd->pw_name);
			if (hack) {
				s = malloc(strlen(p->pwd->pw_dir) +
					sizeof PROJ);
				strcpy(s, p->pwd->pw_dir);
				strcat(s, PROJ);
				if ((fp = fopen(s, "r")) != 0) {
					printf("Project: ");
					while ((c = getc(fp)) != EOF) {
						if (c == '\n')
							break;
						if (isprint(c) || isspace(c))
							putchar(c);
						else
							putchar(c ^ 100);
					}
					fclose(fp);
					putchar('\n');
				}
				free(s);
			}
			if (plan) {
				s = malloc(strlen(p->pwd->pw_dir) +
					sizeof PLAN);
				strcpy(s, p->pwd->pw_dir);
				strcat(s, PLAN);
				if ((fp = fopen(s, "r")) == 0) {
					if (!NONOTHING) printf("No Plan.\n");
				} else {
					printf("Plan:\n");
					while ((c = getc(fp)) != EOF)
						if (isprint(c) || isspace(c))
							putchar(c);
						else
							putchar(c ^ 100);
					fclose(fp);
				}
				free(s);
			}
		}
		if (p->link != 0)
			putchar('\n');
	}
}

/*
 * Duplicate a pwd entry.
 * Note: Only the useful things (what the program currently uses) are copied.
 */
static struct passwd *
pwdcopy(pfrom)
	register struct passwd *pfrom;
{
	register struct passwd *pto;

	pto = (struct passwd *) malloc(sizeof *pto);
#define savestr(s) strcpy(malloc(strlen(s) + 1), s)
	pto->pw_name = savestr(pfrom->pw_name);
	pto->pw_uid = pfrom->pw_uid;
	pto->pw_gecos = savestr(pfrom->pw_gecos);
	pto->pw_dir = savestr(pfrom->pw_dir);
	pto->pw_shell = savestr(pfrom->pw_shell);
#undef savestr
	return pto;
}

/*
 * print out information on quick format giving just name, tty, login time
 * and idle time if idle is set.
 */
static void quickprint(pers)
	register struct person *pers;
{
	printf("%-*.*s  ", NMAX, NMAX, pers->name);
	if (pers->loggedin) {
		if (idle) {
			findidle(pers);
			printf("%c%-*s %-16.16s", pers->writable ? ' ' : '*',
				LMAX, pers->tty, ctime(&pers->loginat));
			ltimeprint("   ", &pers->idletime, "");
		} else
			printf(" %-*s %-16.16s", LMAX,
				pers->tty, ctime(&pers->loginat));
		putchar('\n');
	} else
		printf("          Not Logged In\n");
}

/*
 * print out information in short format, giving login name, full name,
 * tty, idle time, login time, and host.
 */
static void shortprint(pers)
	register struct person *pers;
{
	char *p;
	char dialup;

	if (pers->pwd == 0) {
		printf("%-15s       ???\n", pers->name);
		return;
	}
	printf("%-*s", NMAX, pers->pwd->pw_name);
	dialup = 0;
	if (wide) {
		if (pers->realname)
			printf(" %-20.20s", pers->realname);
		else
			printf("        ???          ");
	}
	putchar(' ');
	if (pers->loggedin && !pers->writable)
		putchar('*');
	else
		putchar(' ');
	if (*pers->tty) {
		if (pers->tty[0] == 't' && pers->tty[1] == 't' &&
		    pers->tty[2] == 'y') {
			if (pers->tty[3] == 'd' && pers->loggedin)
				dialup = 1;
			printf("%-2.2s ", pers->tty + 3);
		} else
			printf("%-2.2s ", pers->tty);
	} else
		printf("   ");
	p = ctime(&pers->loginat);
	if (pers->loggedin) {
		stimeprint(&pers->idletime);
		printf(" %3.3s %-5.5s ", p, p + 11);
	} else if (pers->loginat == 0)
		printf(" < .  .  .  . >");
	else if (tloc - pers->loginat >= 180L * 24 * 60 * 60)
		printf(" <%-6.6s, %-4.4s>", p + 4, p + 20);
	else
		printf(" <%-12.12s>", p + 4);
	if (pers->host[0])
		printf(" %-20.20s", pers->host);
	putchar('\n');
}


/*
 * print out a person in long format giving all possible information.
 * directory and shell are inhibited if unbrief is clear.
 */
static void
personprint(pers)
	register struct person *pers;
{
	if (pers->pwd == 0) {
		printf("Login name: %-10s\t\t\tIn real life: ???\n",
			pers->name);
		return;
	}
	printf("Login name: %-10s", pers->pwd->pw_name);
	if (pers->loggedin && !pers->writable)
		printf("	(messages off)	");
	else
		printf("			");
	if (pers->realname)
		printf("In real life: %s", pers->realname);
	if (unbrief) {
		printf("\nDirectory: %-25s", pers->pwd->pw_dir);
		if (*pers->pwd->pw_shell)
			printf("\tShell: %-s", pers->pwd->pw_shell);
	}
	if (pers->loggedin) {
		register char *ep = ctime(&pers->loginat);
		if (*pers->host) {
			printf("\nOn since %15.15s on %s from %s",
				&ep[4], pers->tty, pers->host);
			ltimeprint("\n", &pers->idletime, " Idle Time");
		} else {
			printf("\nOn since %15.15s on %-*s",
				&ep[4], LMAX, pers->tty);
			ltimeprint("\t", &pers->idletime, " Idle Time");
		}
	} else if (pers->loginat == 0) {
		if (lf >= 0) printf("\nNever logged in.");
	} else if (tloc - pers->loginat > 180L * 24 * 60 * 60) {
		register char *ep = ctime(&pers->loginat);
		printf("\nLast login %10.10s, %4.4s on %s",
			ep, ep+20, pers->tty);
		if (*pers->host)
			printf(" from %s", pers->host);
	} else {
		register char *ep = ctime(&pers->loginat);
		printf("\nLast login %16.16s on %s", ep, pers->tty);
		if (*pers->host)
			printf(" from %s", pers->host);
	}
	putchar('\n');
}


/*
 * decode the information in the gecos field of /etc/passwd
 */
static void
decode(pers)
	register struct person *pers;
{
	char buffer[256];
	register char *bp, *gp, *lp;
	int len;

	pers->realname = 0;
	if (pers->pwd == 0)
		return;
	gp = pers->pwd->pw_gecos;
	bp = buffer;
	if (*gp == ASTERISK)
		gp++;
	while (*gp && *gp != COMMA) 			/* name */
		if (*gp == SAMENAME) {
			lp = pers->pwd->pw_name;
			if (islower(*lp))
				*bp++ = toupper(*lp++);
			while (*bp++ = *lp++)
				;
			bp--;
			gp++;
		} else
			*bp++ = *gp++;
	*bp++ = 0;
	if ((len = bp - buffer) > 1)
		pers->realname = strcpy(malloc(len), buffer);
	if (pers->loggedin)
		findidle(pers);
	else
		findwhen(pers);
}

/*
 * find the last log in of a user by checking the LASTLOG file.
 * the entry is indexed by the uid, so this can only be done if
 * the uid is known (which it isn't in quick mode)
 */

static void
fwopen()
{
	if ((lf = open(LASTLOG, 0)) < 0) {
		if (errno == ENOENT) return;
		fprintf(stderr, "finger: %s open error\n", LASTLOG);
	}
}

static void
findwhen(pers)
	register struct person *pers;
{
	struct utmp ll;
#define ll_line ut_line
#define ll_host ut_host
#define ll_time ut_time

	int i;

	if (lf >= 0) {
		lseek(lf, (long)pers->pwd->pw_uid * sizeof ll, 0);
		if ((i = read(lf, (char *)&ll, sizeof ll)) == sizeof ll) {
			bcopy(ll.ll_line, pers->tty, LMAX);
			pers->tty[LMAX] = 0;
			bcopy(ll.ll_host, pers->host, HMAX);
			pers->host[HMAX] = 0;
			pers->loginat = ll.ll_time;
		} else {
			if (i != 0)
				fprintf(stderr, "finger: %s read error\n",
					LASTLOG);
			pers->tty[0] = 0;
			pers->host[0] = 0;
			pers->loginat = 0L;
		}
	} else {
		pers->tty[0] = 0;
		pers->host[0] = 0;
		pers->loginat = 0L;
	}
}

static void fwclose()
{
	if (lf >= 0)
		close(lf);
}

/*
 * find the idle time of a user by doing a stat on /dev/tty??,
 * where tty?? has been gotten from USERLOG, supposedly.
 */
static void
findidle(pers)
	register struct person *pers;
{
	struct stat ttystatus;
	static char buffer[20] = "/dev/";
	long t;
#define TTYLEN 5

	strcpy(buffer + TTYLEN, pers->tty);
	buffer[TTYLEN+LMAX] = 0;
	if (stat(buffer, &ttystatus) < 0) {
		fprintf(stderr, "finger: Can't stat %s\n", buffer);
		exit(4);
	}
	time(&t);
	if (t < ttystatus.st_atime)
		pers->idletime = 0L;
	else
		pers->idletime = t - ttystatus.st_atime;
	pers->writable = (ttystatus.st_mode & TALKABLE) == TALKABLE;
}

/*
 * print idle time in short format; this program always prints 4 characters;
 * if the idle time is zero, it prints 4 blanks.
 */
static void
stimeprint(dt)
	long *dt;
{
	register struct tm *delta;

	delta = gmtime(dt);
	if (delta->tm_yday == 0)
		if (delta->tm_hour == 0)
			if (delta->tm_min == 0)
				printf("    ");
			else
				printf("  %2d", delta->tm_min);
		else
			if (delta->tm_hour >= 10)
				printf("%3d:", delta->tm_hour);
			else
				printf("%1d:%02d",
					delta->tm_hour, delta->tm_min);
	else
		printf("%3dd", delta->tm_yday);
}

/*
 * print idle time in long format with care being taken not to pluralize
 * 1 minutes or 1 hours or 1 days.
 * print "prefix" first.
 */
static int
ltimeprint(before, dt, after)
	long *dt;
	char *before, *after;
{
	register struct tm *delta;

	delta = gmtime(dt);
	if (delta->tm_yday == 0 && delta->tm_hour == 0 && delta->tm_min == 0 &&
	    delta->tm_sec <= 10)
		return (0);
	printf("%s", before);
	if (delta->tm_yday >= 10)
		printf("%d days", delta->tm_yday);
	else if (delta->tm_yday > 0)
		printf("%d day%s %d hour%s",
			delta->tm_yday, delta->tm_yday == 1 ? "" : "s",
			delta->tm_hour, delta->tm_hour == 1 ? "" : "s");
	else
		if (delta->tm_hour >= 10)
			printf("%d hours", delta->tm_hour);
		else if (delta->tm_hour > 0)
			printf("%d hour%s %d minute%s",
				delta->tm_hour, delta->tm_hour == 1 ? "" : "s",
				delta->tm_min, delta->tm_min == 1 ? "" : "s");
		else
			if (delta->tm_min >= 10)
				printf("%2d minutes", delta->tm_min);
			else if (delta->tm_min == 0)
				printf("%2d seconds", delta->tm_sec);
			else
				printf("%d minute%s %d second%s",
					delta->tm_min,
					delta->tm_min == 1 ? "" : "s",
					delta->tm_sec,
					delta->tm_sec == 1 ? "" : "s");
	printf("%s", after);
}

static int
matchcmp(gname, login, given)
	register char *gname;
	char *login;
	char *given;
{
	char buffer[100];
	register char *bp, *lp;
	register c;

	if (*gname == ASTERISK)
		gname++;
	lp = 0;
	bp = buffer;
	for (;;)
		switch (c = *gname++) {
		case SAMENAME:
			for (lp = login; bp < buffer + sizeof buffer
					 && (*bp++ = *lp++);)
				;
			bp--;
			break;
		case ' ':
		case COMMA:
		case '\0':
			*bp = 0;
			if (namecmp(buffer, given))
				return (1);
			if (c == COMMA || c == 0)
				return (0);
			bp = buffer;
			break;
		default:
			if (bp < buffer + sizeof buffer)
				*bp++ = c;
		}
	/*NOTREACHED*/
}

static int
namecmp(name1, name2)
	register char *name1, *name2;
{
	register c1, c2;

	for (;;) {
		c1 = *name1++;
		if (islower(c1))
			c1 = toupper(c1);
		c2 = *name2++;
		if (islower(c2))
			c2 = toupper(c2);
		if (c1 != c2)
			break;
		if (c1 == 0)
			return (1);
	}
	if (!c1) {
		for (name2--; isdigit(*name2); name2++)
			;
		if (*name2 == 0)
			return (1);
	} else if (!c2) {
		for (name1--; isdigit(*name1); name1++)
			;
		if (*name2 == 0)
			return (1);
	}
	return (0);
}

#if NONET
static int
netfinger(name)
char *name;
{
	return 0;
}
#else
static int
netfinger(name)
	char *name;
{
	char *host;
	struct hostent *hp;
	int s, result;
#if !defined(__minix)
	char *rindex();
#endif
	register FILE *f;
	register int c;
	register int lastc;
	nwio_tcpconf_t tcpconf;
	nwio_tcpcl_t tcpconnopt;
	char *tcp_device;

	if (name == NULL)
		return (0);
	host = rindex(name, '@');
	if (host == NULL)
		return (0);
	*host++ = 0;
	hp = gethostbyname(host);
	if (hp == NULL) {
		static struct hostent def;
		static ipaddr_t defaddr;
		static char namebuf[128];

		defaddr = inet_addr(host);
		if (defaddr == -1) {
			printf("unknown host: %s\n", host);
			return (1);
		}
		strcpy(namebuf, host);
		def.h_name = namebuf;
		def.h_addr = (char *)&defaddr;
		def.h_length = sizeof (ipaddr_t);
		def.h_addrtype = AF_INET;
		def.h_aliases = 0;
		hp = &def;
	}
	printf("[%s] ", hp->h_name);
	fflush(stdout);

	tcp_device= getenv("TCP_DEVICE");
	if (tcp_device == NULL)
		tcp_device= TCP_DEVICE;
	s= open (tcp_device, O_RDWR);
	if (s == -1)
	{
		fprintf(stderr, "%s: unable to open %s (%s)\n",
			prog_name, tcp_device, strerror(errno));
		exit(1);
	}
	tcpconf.nwtc_flags= NWTC_LP_SEL | NWTC_SET_RA | NWTC_SET_RP;
	tcpconf.nwtc_remaddr= *(ipaddr_t *)hp->h_addr;
	tcpconf.nwtc_remport= htons(TCPPORT_FINGER);

	result= ioctl (s, NWIOSTCPCONF, &tcpconf);
	if (result<0)
	{
		fprintf(stderr, "%s\n", strerror(errno));
		exit(1);
	}

	tcpconnopt.nwtcl_flags= 0;

	do
	{
		result= ioctl (s, NWIOTCPCONN, &tcpconnopt);
		if (result<0 && errno== EAGAIN)
		{
			fprintf(stderr, "got EAGAIN error, sleeping 2s\n");
			sleep(2);
		}
	} while (result<0 && errno == EAGAIN);
	if (result<0)
	{
		fprintf(stderr, "%s\n", strerror(errno));
		exit(1);
	}
	printf("\r\n");
	if (large) write(s, "/W ", 3);
	write(s, name, strlen(name));
	write(s, "\r\n", 2);
	f = fdopen(s, "r");
	while ((c = getc(f)) != EOF) {
/*
		switch(c) {
		case 0210:
		case 0211:
		case 0212:
		case 0214:
			c -= 0200;
			break;
		case 0215:
			c = '\n';
			break;
		}
*/
		c &= ~0200;
		if (c == '\r')
		{
			c= getc(f) & ~0200;
			if (c == '\012')
			{
				lastc= c;
				putchar('\n');
				continue;
			}
			else
				putchar('\r');
		}
		lastc = c;
		if (isprint(c) || isspace(c))
			putchar(c);
		else
			putchar(c ^ 100);
	}
	if (lastc != '\n')
		putchar('\n');
	(void)fclose(f);
	return (1);
}
#endif

/*
 *	AnyMail - takes a username (string pointer thereto), and
 *	prints on standard output whether there is any unread mail,
 *	and if so, how old it is.	(JCM@Shasta 15 March 80)
 */
#define preamble "/usr/spool/mail/"	/* mailboxes are there */
static int
AnyMail(name)
char *name;
{
	struct stat buf;		/* space for file status buffer */
	char *mbxdir = preamble; 	/* string with path preamble */
	char *mbxpath;			/* space for entire pathname */

#if !defined(__minix)
	char *ctime();			/* convert longword time to ascii */
#endif
	char *timestr;

	mbxpath = malloc(strlen(name) + strlen(preamble) + 1);

	strcpy(mbxpath, mbxdir);	/* copy preamble into path name */
	strcat(mbxpath, name);		/* concatenate user name to path */

	if (stat(mbxpath, &buf) == -1 || buf.st_size == 0) {
	    /* Mailbox is empty or nonexistent */
	    if (!NONOTHING) printf("No unread mail\n");
        } else {
	    if (buf.st_mtime == buf.st_atime) {
		/* There is something in the mailbox, but we can't really
		 *   be sure whether it is mail held there by the user
		 *   or a (single) new message that was placed in a newly
		 *   recreated mailbox, so we punt and call it "unread mail."
		 */
		printf("Unread mail since ");
	        printf(ctime(&buf.st_mtime));
	    } else {
		/* New mail has definitely arrived since the last time
		 *   mail was read.  mtime is the time the most recent
		 *   message arrived; atime is either the time the oldest
		 *   unread message arrived, or the last time the mail
		 *   was read.
		 */
		printf("New mail received ");
		timestr = ctime(&buf.st_mtime);	/* time last modified */
		timestr[24] = '\0';		/* suppress newline (ugh) */
		printf(timestr);
		printf(";\n  unread since ");
	        printf(ctime(&buf.st_atime));	/* time last accessed */
	    }
	}
	
	free(mbxpath);
}

/*
 * return true iff we've already printed project/plan for this uid;
 * if not, enter this uid into table (so this function has a side-effect.)
 */
#define	PPMAX	200		/* assume no more than 200 logged-in users */
int	PlanPrinted[PPMAX+1];
int	PPIndex = 0;		/* index of next unused table entry */

static int
AlreadyPrinted(uid)
int uid;
{
	int i = 0;
	
	while (i++ < PPIndex) {
	    if (PlanPrinted[i] == uid)
		return(1);
	}
	if (i < PPMAX) {
	    PlanPrinted[i] = uid;
	    PPIndex++;
	}
	return(0);
}
