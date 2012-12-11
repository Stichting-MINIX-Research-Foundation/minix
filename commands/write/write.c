/* write - write to a logged in user	Authors: N. Andrew and F. van Kempen */

/*
 * Usage:	write [-c] [-v] user [tty]
 *  			-c Read & write one character at a time (cbreak mode)
 *			-v Verbose
 *
 * Version:	1.6	10/24/92
 *
 * NOTES:	Write requires 1.4a (or higher) libraries,
 *		for getopt(), strchr().
 *
 * Authors:	Nick Andrew  (nick@nswitgould.oz)  - Public Domain
 *		Fred van Kempen (minixug!waltje@kyber.uucp)
 */

#include <sys/types.h>
#include <fcntl.h>
#include <pwd.h>
#include <termios.h>
#include <signal.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <utmp.h>
#include <time.h>
#include <stdio.h>
#include <paths.h>

#if 0
static char *Version = "@(#) WRITE 1.6 (10/24/92)";
#endif

int otty;			/* file desc of callee's terminal */
short int cbreak = 0;		/* are we in CBREAK (-c) mode? */
short int verbose = 0;		/* are we in VERBOSE (-v) mode? */
short int writing = 0;		/* is there a connection? */
char *user = NULL;	/* callee's user name */
char *tty = NULL;	/* callee's terminal if given */
char *ourtty = NULL;	/* our terminal name */
struct termios ttyold, ttynew;	/* our tty controlling structs */

extern int optind;

int main(int argc, char **argv);
char *finduser(void);
void settty(char *utty);
void sayhello(void);
void escape(char *cmd);
void writetty(void);
void usage(void);
void intr(int dummy);

char *finduser()
{
/* Search the UTMP database for the user we want. */

  static char utmptty[16];
  struct utmp utmp;
  struct passwd *userptr;
  int utmpfd;

  ourtty = ttyname(0);
  if (ourtty == NULL) ourtty = "/dev/console";

  if (user == NULL) exit(-1);
  if ((userptr = getpwnam(user)) == NULL) {
	fprintf(stderr, "No such user: %s\n", user);
	return(NULL);
  }
  if (verbose) fprintf(stderr, "Trying to write to %s\n",
		userptr->pw_gecos);

  if ((utmpfd = open(_PATH_UTMP, O_RDONLY)) < 0) {
	fprintf(stderr, "Cannot open utmp file\n");
	return(NULL);
  }
  utmptty[0] = '\0';

  /* We want to find if 'user' is logged on, and return in utmptty[]
   * 'user' `s terminal, and if 'user' is logged onto the tty the
   * caller specified, return that tty name. */
  while (read(utmpfd, (char *) &utmp, sizeof(utmp)) == sizeof(utmp)) {
	/* is this the user we are looking for? */
	if (strncmp(utmp.ut_name, user, sizeof(utmp.ut_name))) continue;

	strcpy(utmptty, utmp.ut_line);
	/* is he on the terminal we want to write to? */
	if (tty == NULL || !strcmp(utmptty, tty)) {
		break;
	}
  }

  if (utmptty[0] == '\0') {
	fprintf(stderr, "%s is not logged on\n", user);
	return( NULL);
  }
  if (tty != NULL && strcmp(utmptty, tty)) {
	fprintf(stderr, "%s is logged onto %s, not %s\n", user, utmptty, tty);
	return( NULL);
  }
  close(utmpfd);

  if (verbose) fprintf(stderr, "Writing to %s on %s\n", user, utmptty);
  return(utmptty);
}


void settty(utty)
char *utty;			/* name of terminal found in utmp */
{
/* Open other person's terminal and setup our own terminal. */

  char buff[48];

  sprintf(buff, "/dev/%s", utty);
  if ((otty = open(buff, O_WRONLY)) < 0) {
	fprintf(stderr, "Cannot open %s to write to %s\n", utty, user);
	fprintf(stderr, "It may have write permission turned off\n");
	exit(-1);
  }
  tcgetattr(0, &ttyold);
  tcgetattr(0, &ttynew);
  ttynew.c_lflag &= ~(ICANON|ECHO);
  signal(SIGINT, intr);
  if (cbreak) tcsetattr(0, TCSANOW, &ttynew);
}


void sayhello()
{
  struct passwd *pw;
  char buff[128];
  time_t now;
  char *sp;

  time(&now);

  pw = getpwuid(getuid());
  if (pw == NULL) {
	fprintf(stderr, "unknown user\n");
	exit(-1);
  }
  if ((sp = strrchr(ourtty, '/')) != NULL)
	++sp;
  else
	sp = ourtty;

  sprintf(buff, "\nMessage from %s (%s) %-24.24s...\n",
	pw->pw_name, sp, ctime(&now));

  write(otty, buff, strlen(buff));
  printf("\007\007");
  fflush(stdout);
}


void escape(cmd)
char *cmd;
{
/* Shell escape. */

  register char *x;

  write(1, "!\n", 2);
  for (x = cmd; *x; ++x)
	if (*x == '\n') *x = '\0';

  system(cmd);
  write(1, "!\n", 2);
}


void writetty()
{
/* The write loop. */

  char line[80];
  int n, cb_esc;

  writing = 1;
  cb_esc = 0;

  while ((n = read(0, line, 79)) > 0) {
	if (line[0] == '\004') break;	/* EOT */

	if (cbreak && line[0] == '\n') cb_esc = 1;

	if (cbreak) write(1, line, n);

	if (line[0] == '!') {
		if (cbreak && cb_esc) {
			cb_esc = 0;
			tcsetattr(0, TCSANOW, &ttyold);
			read(0, line, 79);
			escape(line);
			tcsetattr(0, TCSANOW, &ttynew);
		} else if (cbreak)
			write(otty, line, n);
		else
			escape(&line[1]);
		continue;
	}
	write(otty, line, n);
  }
  write(1, "\nEOT\n", 5);
  write(otty, "\nEOT\n", 5);
}


void usage()
{
  fprintf(stderr, "usage: write [-c] [-v] user [tty]\n");
  fprintf(stderr, "\t-c : cbreak mode\n\t-v : verbose\n");
  exit(-1);
}


int main(argc, argv)
int argc;
char *argv[];
{
  register int c;
  char *sp;

  setbuf(stdout, (char *) NULL);

  /* Parse options. */
  while ((c = getopt(argc, argv, "cv")) != EOF) switch (c) {
	        case 'c':	cbreak = 1;	break;
	        case 'v':	verbose = 1;	break;
	    default:
		usage();
	}

  /* Parse user and tty arguments */
  if (optind < argc) {
	user = argv[optind++];

	/* WTMP usernames are 1-8 chars */
	if (strlen(user) > 8) *(user + 8) = '\0';

	if (optind < argc) {
		tty = argv[optind++];
		if (optind < argc) usage();
	}
  } else
	usage();

  sp = finduser();		/* find which tty to write onto */
  if (sp != NULL) {	/* did we find one? */
	settty(sp);		/* setup our terminal */
	sayhello();		/* print the initial message */
	writetty();		/* the write loop */
	tcsetattr(0, TCSANOW, &ttyold);
	exit(0);
  }
  return(-1);
}

void intr(dummy)
int dummy;			/* to satisfy the prototype */
{
/* The interrupt key has been hit. exit cleanly. */

  signal(SIGINT, SIG_IGN);
  fprintf(stderr, "\nInterrupt. Exiting write\n");
  tcsetattr(0, TCSANOW, &ttyold);
  if (writing) write(otty, "\nEOT\n", 5);
  exit(0);
}
