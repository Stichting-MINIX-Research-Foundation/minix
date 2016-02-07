/*  mail - send/receive mail 		 Author: Peter S. Housel */
/* Version 0.2 of September 1990: added -e, -t, * options - cwr */

/* 2003-07-18: added -s option - ASW */

#include <sys/types.h>
#include <sys/stat.h>
#include <errno.h>
#undef EOF			/* temporary hack */
#include <signal.h>
#include <pwd.h>
#include <time.h>
#include <setjmp.h>
#include <string.h>
#include <stdlib.h>
#include <fcntl.h>
#include <unistd.h>
#include <sys/wait.h>
#include <stdio.h>

#ifdef DEBUG
#define D(Q) (Q)
#else
#define D(Q)
#endif

#define SHELL		"/bin/sh"

#define DROPNAME 	"/var/mail/%s"
#define LOCKNAME	"/var/mail/%s.lock"
#define LOCKWAIT	5	/* seconds to wait after collision */
#define LOCKTRIES	4	/* maximum number of collisions */

#define MBOX		"mbox"

#define HELPFILE	"/usr/lib/mail.help"
#define PROMPT		"? "
#define PATHLEN		80
#define MAXRCPT		100	/* maximum number of recipients */
#define LINELEN		512

/* #define MAILER		"/usr/bin/smail"	*/ /* smart mailer */
#define MAILERARGS		/* (unused) */

#define UNREAD		1	/* 'not read yet' status */
#define DELETED		2	/* 'deleted' status */
#define READ		3	/* 'has been read' status */

struct letter {
  struct letter *prev, *next;	/* linked letter list */
  int status;			/* letter status */
  off_t location;		/* location within mailbox file */
};

struct letter *firstlet, *lastlet;

int usemailer = 1;		/* use MAILER to deliver (if any) */
int printmode = 0;		/* print-and-exit mode */
int quitmode = 0;		/* take interrupts */
int reversemode = 0;		/* print mailbox in reverse order */
int usedrop = 1;		/* read the maildrop (no -f given) */
int verbose = 0;		/* pass "-v" flag on to mailer */
int needupdate = 0;		/* need to update mailbox */
int msgstatus = 0;		/* return the mail status */
int distlist = 0;		/* include distribution list */
char mailbox[PATHLEN];		/* user's mailbox/maildrop */
char tempname[PATHLEN] = "/tmp/mailXXXXXX";	/* temporary file */
char *subject = NULL;
FILE *boxfp = NULL;		/* mailbox file */
jmp_buf printjump;		/* for quitting out of letters */
unsigned oldmask;		/* saved umask() */

extern int optind;
extern char *optarg;

int main(int argc, char **argv);
int deliver(int count, char *vec []);
FILE *makerewindable(void);
int copy(FILE *fromfp, FILE *tofp);
void readbox(void);
void printall(void);
void interact(void);
void onint(int dummy);
void savelet(struct letter *let, char *savefile);
void updatebox(void);
void printlet(struct letter *let, FILE *tofp);
void doshell(char *command);
void usage(void);
char *basename(char *name);
char *whoami(void);
void dohelp(void);
int filesize(char *name);

int main(argc, argv)
int argc;
char *argv[];
{
  int c;

  if ('l' == (basename(argv[0]))[0])	/* 'lmail' link? */
	usemailer = 0;		/* yes, let's deliver it */

  (void) mktemp(tempname);	/* name the temp file */

  oldmask = umask(022);		/* change umask for security */

  while (EOF != (c = getopt(argc, argv, "epqrf:tdvs:"))) switch (c) {
	    case 'e':	++msgstatus;	break;	

	    case 't':	++distlist;	break;

	    case 'p':	++printmode;	break;

	    case 'q':	++quitmode;	break;

	    case 'r':	++reversemode;	break;

	    case 'f':
		setuid(getuid());	/* won't need to lock */
		usedrop = 0;
		strncpy(mailbox, optarg, (size_t)(PATHLEN - 1));
		break;

	    case 'd':	usemailer = 0;	break;

	    case 'v':	++verbose;	break;

	    case 's':	subject = optarg; break;

	    default:
		usage();
		exit(1);
	}

  if (optind < argc) {
	if (deliver(argc - optind, argv + optind) < 0)
		exit(1);
	else
		exit(0);
  }
  if (usedrop) sprintf(mailbox, DROPNAME, whoami());

  D(printf("mailbox=%s\n", mailbox));

  if (msgstatus) {
	if (filesize(mailbox))
		exit(0);
	else
		exit(1);
  }

  readbox();

  if (printmode)
	printall();
  else
	interact();

  if (needupdate) updatebox();

  return(0);
}

int deliver(count, vec)
int count;
char *vec[];
{
  int i, j;
  int errs = 0;			/* count of errors */
  int dropfd;			/* file descriptor for user's drop */
  int created = 0;		/* true if we created the maildrop */
  FILE *mailfp;			/* fp for mail */
  struct stat stb;		/* for checking drop modes, owners */
#ifdef __STDC__
  void (*sigint)(int), (*sighup)(int), (*sigquit)(int);/* saving signal state */
#else
  void (*sigint) (), (*sighup) (), (*sigquit) ();      /* saving signal state */
#endif
  time_t now;			/* for datestamping the postmark */
  char sender[32];		/* sender's login name */
  char lockname[PATHLEN];	/* maildrop lock */
  int locktries;		/* tries when box is locked */
  struct passwd *pw;		/* sender and recipent */
  int to_console;		/* deliver to console if everything fails */

  if (count > MAXRCPT) {
	fprintf(stderr, "mail: too many recipients\n");
	return -1;
  }
#ifdef MAILER
  if (usemailer) {
	char *argvec[MAXRCPT + 3];
	char **argp;

	setuid(getuid());

	argp = argvec;
	*argp++ = "send-mail";
	if (verbose) *argp++ = "-v";

	for (i = 0; i < count; ++i) *argp++ = vec[i];

	*argp = NULL;
	execv(MAILER, argvec);
	fprintf(stderr, "mail: couldn't exec %s\n", MAILER);
	return -1;
  }
#endif /* MAILER */

  if (NULL == (pw = getpwuid(getuid()))) {
	fprintf(stderr, "mail: unknown sender\n");
	return -1;
  }
  strcpy(sender, pw->pw_name);

  /* If we need to rewind stdin and it isn't rewindable, make a copy */
  if (isatty(0) || (count > 1 && lseek(0, 0L, 0) == (off_t) -1)) {
	mailfp = makerewindable();
  } else
	mailfp = stdin;

  /* Shut off signals during the delivery */
  sigint = signal(SIGINT, SIG_IGN);
  sighup = signal(SIGHUP, SIG_IGN);
  sigquit = signal(SIGQUIT, SIG_IGN);

  for (i = 0; i < count; ++i) {
	if (count > 1) rewind(mailfp);

	D(printf("deliver to %s\n", vec[i]));

	if (NULL == (pw = getpwnam(vec[i]))) {
		fprintf(stderr, "mail: user %s not known\n", vec[i]);
		++errs;
		continue;
	}
	sprintf(mailbox, DROPNAME, pw->pw_name);
	sprintf(lockname, LOCKNAME, pw->pw_name);

	D(printf("maildrop='%s', lock='%s'\n", mailbox, lockname));

	/* Lock the maildrop while we're messing with it. Races are
	 * possible (though not very likely) when we have to create
	 * the maildrop, but not otherwise. If the box is already
	 * locked, wait awhile and try again. */
	locktries = created = to_console = 0;
trylock:
	if (link(mailbox, lockname) != 0) {
		if (ENOENT == errno) {	/* user doesn't have a drop yet */
			dropfd = creat(mailbox, 0600);
			if (dropfd < 0 && errno == ENOENT) {
				/* Probably missing spool dir; to console. */
				boxfp = fopen("/dev/console", "w");
				if (boxfp != NULL) {
					to_console = 1;
					goto nobox;
				}
			}
			if (dropfd < 0) {
				fprintf(stderr, "mail: couln't create a maildrop for user %s\n",
					vec[i]);
				++errs;
				continue;
			}
			++created;
			goto trylock;
		} else {	/* somebody else has it locked, it seems -
			 * wait */
			if (++locktries >= LOCKTRIES) {
				fprintf(stderr, "mail: couldn't lock maildrop for user %s\n",
					vec[i]);
				++errs;
				continue;
			}
			sleep(LOCKWAIT);
			goto trylock;
		}
	}
	if (created) {
		(void) chown(mailbox, pw->pw_uid, pw->pw_gid);
		boxfp = fdopen(dropfd, "a");
	} else
		boxfp = fopen(mailbox, "a");

	if (NULL == boxfp || stat(mailbox, &stb) < 0) {
		fprintf(stderr, "mail: serious maildrop problems for %s\n", vec[i]);
		unlink(lockname);
		++errs;
		continue;
	}
	if (stb.st_uid != pw->pw_uid || (stb.st_mode & S_IFMT) != S_IFREG) {
		fprintf(stderr, "mail: mailbox for user %s is illegal\n", vec[i]);
		unlink(lockname);
		++errs;
		continue;
	}
nobox:
	if (to_console) {
		fprintf(boxfp,
			"-------------\n| Mail from %s to %s\n-------------\n",
			sender, vec[i]);
	} else {
		(void) time(&now);
		fprintf(boxfp, "From %s %24.24s\n", sender, ctime(&now));
	}

	/* Add the To: header line */
	fprintf(boxfp, "To: %s\n", vec[i]);

	if (distlist) {
		fprintf(boxfp, "Dist: ");
		for (j = 0; j < count; ++j)
			if (getpwnam(vec[j]) != NULL && j != i)
				fprintf(boxfp, "%s ", vec[j]) ;
		fprintf(boxfp, "\n");
	}

	/* Add the Subject: header line */
	if (subject != NULL) fprintf(boxfp, "Subject: %s\n", subject);

	fprintf(boxfp, "\n");
 
	if ((copy(mailfp, boxfp) < 0) || (fclose(boxfp) != 0)) {
		fprintf(stderr, "mail: error delivering to user %s", vec[i]);
		perror(" ");
		++errs;
	}
	unlink(lockname);
  }

  fclose(mailfp);

  /* Put signals back the way they were */
  signal(SIGINT, sigint);
  signal(SIGHUP, sighup);
  signal(SIGQUIT, sigquit);

  return(0 == errs) ? 0 : -1;
}

/* 'stdin' isn't rewindable. Make a temp file that is.
 * Note that if one wanted to catch SIGINT and write a '~/dead.letter'
 * for interactive mails, this might be the place to do it (though the
 * case where a MAILER is being used would also need to be handled).
 */
FILE *makerewindable()
{
  FILE *tempfp;			/* temp file used for copy */
  int c;			/* character being copied */
  int state;			/* ".\n" detection state */

  if (NULL == (tempfp = fopen(tempname, "w"))) {
	fprintf(stderr, "mail: can't create temporary file\n");
	return NULL;
  }

  /* Here we copy until we reach the end of the letter (end of file or
   * a line containing only a '.'), painstakingly avoiding setting a
   * line length limit. */
  state = '\n';
  while (EOF != (c = getc(stdin))) switch (state) {
	    case '\n':
		if ('.' == c)
			state = '.';
		else {
			if ('\n' != c) state = '\0';
			putc(c, tempfp);
		}
		break;
	    case '.':
		if ('\n' == c) goto done;
		state = '\0';
		putc('.', tempfp);
		putc(c, tempfp);
		break;
	    default:
		state = ('\n' == c) ? '\n' : '\0';
		putc(c, tempfp);
	}
done:
  if (ferror(tempfp) || fclose(tempfp)) {
	fprintf(stderr, "mail: couldn't copy letter to temporary file\n");
	return NULL;
  }
  tempfp = freopen(tempname, "r", stdin);
  unlink(tempname);		/* unlink name; file lingers on in limbo */
  return tempfp;
}

int copy(fromfp, tofp)
FILE *fromfp, *tofp;
{
  int c;			/* character being copied */
  int state;			/* ".\n" and postmark detection state */
  int blankline = 0;		/* was most recent line completely blank? */
  static char postmark[] = "From ";
  char *p, *q;

  /* Here we copy until we reach the end of the letter (end of file or
   * a line containing only a '.'). Postmarks (lines beginning with
   * "From ") are copied with a ">" prepended. Here we also complicate
   * things by not setting a line limit. */
  state = '\n';
  p = postmark;
  while (EOF != (c = getc(fromfp))) {
	switch (state) {
	    case '\n':
		if ('.' == c)	/* '.' at BOL */
			state = '.';
		else if (*p == c) {	/* start of postmark */
			++p;
			state = 'P';
		} else {	/* anything else */
			if ('\n' == c)
				blankline = 1;
			else {
				state = '\0';
				blankline = 0;
			}
			putc(c, tofp);
		}
		break;
	    case '.':
		if ('\n' == c) goto done;
		state = '\0';
		putc('.', tofp);
		putc(c, tofp);
		break;
	    case 'P':
		if (*p == c) {
			if (*++p == '\0') {	/* successfully reached end */
				p = postmark;
				putc('>', tofp);
				fputs(postmark, tofp);
				state = '\0';
				break;
			}
			break;	/* not there yet */
		}
		state = ('\n' == c) ? '\n' : '\0';
		for (q = postmark; q < p; ++q) putc(*q, tofp);
		putc(c, tofp);
		blankline = 0;
		p = postmark;
		break;
	    default:
		state = ('\n' == c) ? '\n' : '\0';
		putc(c, tofp);
	}
  }
  if ('\n' != state) putc('\n', tofp);
done:
  if (!blankline) putc('\n', tofp);
  if (ferror(tofp)) return -1;
  return 0;
}

void readbox()
{
  char linebuf[512];
  struct letter *let;
  off_t current;

  firstlet = lastlet = NULL;

  if (access(mailbox, 4) < 0 || NULL == (boxfp = fopen(mailbox, "r"))) {
	if (usedrop && ENOENT == errno) return;
	fprintf(stderr, "can't access mailbox ");
	perror(mailbox);
	exit(1);
  }
  current = 0L;
  while (1) {
	if (NULL == fgets(linebuf, sizeof linebuf, boxfp)) break;

	if (!strncmp(linebuf, "From ", (size_t)5)) {
		if (NULL == (let = (struct letter *) malloc(sizeof(struct letter)))) {
			fprintf(stderr, "Out of memory.\n");
			exit(1);
		}
		if (NULL == lastlet) {
			firstlet = let;
			let->prev = NULL;
		} else {
			let->prev = lastlet;
			lastlet->next = let;
		}
		lastlet = let;
		let->next = NULL;

		let->status = UNREAD;
		let->location = current;
		D(printf("letter at %ld\n", current));
	}
	current += strlen(linebuf);
  }
}

void printall()
{
  struct letter *let;

  let = reversemode ? firstlet : lastlet;

  if (NULL == let) {
	printf("No mail.\n");
	return;
  }
  while (NULL != let) {
	printlet(let, stdout);
	let = reversemode ? let->next : let->prev;
  }
}

void interact()
{
  char linebuf[512];		/* user input line */
  struct letter *let, *next;	/* current and next letter */
  int interrupted = 0;		/* SIGINT hit during letter print */
  int needprint = 1;		/* need to print this letter */
  char *savefile;		/* filename to save into */

  if (NULL == firstlet) {
	printf("No mail.\n");
	return;
  }
  let = reversemode ? firstlet : lastlet;

  while (1) {
	next = reversemode ? let->next : let->prev;
	if (NULL == next) next = let;

	if (!quitmode) {
		interrupted = setjmp(printjump);
		signal(SIGINT, onint);
	}
	if (!interrupted && needprint) {
		if (DELETED != let->status) let->status = READ;
		printlet(let, stdout);
	}
	if (interrupted) putchar('\n');
	needprint = 0;
	fputs(PROMPT, stdout);
	fflush(stdout);

	if (fgets(linebuf, sizeof linebuf, stdin) == NULL) break;

	if (!quitmode) signal(SIGINT, SIG_IGN);

	switch (linebuf[0]) {
	    case '\n':
		let = next;
		needprint = 1;
		continue;
	    case 'd':
		let->status = DELETED;
		if (next != let)/* look into this */
			needprint = 1;
		needupdate = 1;
		let = next;
		continue;
	    case 'p':
		needprint = 1;
		continue;
	    case '-':
		next = reversemode ? let->prev : let->next;
		if (NULL == next) next = let;
		let = next;
		needprint = 1;
		continue;
	    case 's':
		for (savefile = strtok(linebuf + 1, " \t\n");
		     savefile != NULL;
		     savefile = strtok((char *) NULL, " \t\n")) {
			savelet(let, savefile);
		}
		continue;
	    case '!':
		doshell(linebuf + 1);
		continue;
	    case '*':
		dohelp();
		continue;
	    case 'q':
		return;
	    case 'x':
		exit(0);
	    default:
		fprintf(stderr, "Illegal command\n");
		continue;
	}
  }
}

void onint(dummy)
int dummy;	/* to satisfy ANSI compilers */
{
  longjmp(printjump, 1);
}

void savelet(let, savefile)
struct letter *let;
char *savefile;
{
  int waitstat, pid;
  FILE *savefp;

  if ((pid = fork()) < 0) {
	perror("mail: couldn't fork");
	return;
  } else if (pid != 0) {	/* parent */
	wait(&waitstat);
	return;
  }

  /* Child */
  setgid(getgid());
  setuid(getuid());
  if ((savefp = fopen(savefile, "a")) == NULL) {
	perror(savefile);
	exit(0);
  }
  printlet(let, savefp);
  if ((ferror(savefp) != 0) | (fclose(savefp) != 0)) {
	fprintf(stderr, "savefile write error:");
	perror(savefile);
  }
  exit(0);
}

void updatebox()
{
  FILE *tempfp;			/* fp for tempfile */
  char lockname[PATHLEN];	/* maildrop lock */
  int locktries = 0;		/* tries when box is locked */
  struct letter *let;		/* current letter */
  int c;

  sprintf(lockname, LOCKNAME, whoami());

  if (NULL == (tempfp = fopen(tempname, "w"))) {
	perror("mail: can't create temporary file");
	return;
  }
  for (let = firstlet; let != NULL; let = let->next) {
	if (let->status != DELETED) {
		printlet(let, tempfp);
		D(printf("printed letter at %ld\n", let->location));
	}
  }

  if (ferror(tempfp) || NULL == (tempfp = freopen(tempname, "r", tempfp))) {
	perror("mail: temporary file write error");
	unlink(tempname);
	return;
  }

  /* Shut off signals during the update */
  signal(SIGINT, SIG_IGN);
  signal(SIGHUP, SIG_IGN);
  signal(SIGQUIT, SIG_IGN);

  if (usedrop) while (link(mailbox, lockname) != 0) {
		if (++locktries >= LOCKTRIES) {
			fprintf(stderr, "mail: couldn't lock maildrop for update\n");
			return;
		}
		sleep(LOCKWAIT);
	}

  if (NULL == (boxfp = freopen(mailbox, "w", boxfp))) {
	perror("mail: couldn't reopen maildrop");
	fprintf(stderr, "mail may have been lost; look in %s\n", tempname);
	if (usedrop) unlink(lockname);
	return;
  }
  unlink(tempname);

  while ((c = getc(tempfp)) != EOF) putc(c, boxfp);

  fclose(boxfp);

  if (usedrop) unlink(lockname);
}

void printlet(let, tofp)
struct letter *let;
FILE *tofp;
{
  off_t current, limit;
  int c;

  fseek(boxfp, (current = let->location), 0);
  limit = (NULL != let->next) ? let->next->location : -1;

  while (current != limit && (c = getc(boxfp)) != EOF) {
	putc(c, tofp);
	++current;
  }
}

void doshell(command)
char *command;
{
  int waitstat, pid;
  char *shell;

  if (NULL == (shell = getenv("SHELL"))) shell = SHELL;

  if ((pid = fork()) < 0) {
	perror("mail: couldn't fork");
	return;
  } else if (pid != 0) {	/* parent */
	wait(&waitstat);
	return;
  }

  /* Child */
  setgid(getgid());
  setuid(getuid());
  umask(oldmask);

  execl(shell, shell, "-c", command, (char *) NULL);
  fprintf(stderr, "can't exec shell\n");
  exit(127);
}

void usage()
{
  fprintf(stderr, "usage: mail [-epqr] [-f file]\n");
  fprintf(stderr, "       mail [-dtv] [-s subject] user [...]\n");
}

char *basename(name)
char *name;
{
  char *p;

  if (NULL == (p = rindex(name, '/')))
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

void dohelp()
{
  FILE *fp;
  char buffer[80];

  if ( (fp = fopen(HELPFILE, "r")) == NULL) {
	fprintf(stdout, "can't open helpfile %s\n", HELPFILE);
	return;
  }

  while (fgets(buffer, 80, fp))
	fputs(buffer, stdout);

  fclose(fp);
}

int filesize(name)
char *name ;
{
  struct stat buf;
 
  if (stat(name, &buf) == -1)
	buf.st_size = 0L;

  return (buf.st_size ? 1 : 0);
}
