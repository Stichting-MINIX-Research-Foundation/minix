/* login - log into the system			Author: Patrick van Kleef */

/* Original version by Patrick van Kleef.  History of modifications:
 *
 * Peter S. Housel   Jan. 1988
 *  - Set up $USER, $HOME and $TERM.
 *  - Set signals to SIG_DFL.
 *
 * Terrence W. Holm   June 1988
 *  - Allow a username as an optional argument.
 *  - Time out if a password is not typed within 60 seconds.
 *  - Perform a dummy delay after a bad username is entered.
 *  - Don't allow a login if "/etc/nologin" exists.
 *  - Cause a failure on bad "pw_shell" fields.
 *  - Record the login in "/usr/adm/wtmp".
 *
 * Peter S. Housel   Dec. 1988
 *  - Record the login in "/etc/utmp" also.
 *
 * F. van Kempen     June 1989
 *  - various patches for Minix V1.4a.
 *
 * F. van Kempen     September 1989
 *  - added login-failure administration (new utmp.h needed!).
 *  - support arguments in pw_shell field
 *  - adapted source text to MINIX Style Sheet
 *
 * F. van Kempen     October 1989
 *  - adapted to new utmp database.
 * F. van Kempen,    December 1989
 *  - fixed 'slot' assumption in wtmp()
 *  - fixed all MSS-stuff
 *  - adapted to POSIX (MINIX 1.5)
 * F. van Kempen,    January 1990
 *  - made all 'bad login accounting' optional by "#ifdef BADLOG".
 * F. van Kempen,    Februari 1990
 *  - fixed 'first argument' bug and added some casts.
 *
 * Andy Tanenbaum April 1990
 * - if /bin/sh cannot be located, try /usr/bin/sh
 *
 * Michael A. Temari	October 1990
 *  - handle more than single digit tty devices
 *
 * Philip Homburg - Feb 28 1992
 *  - use ttyname to get the name of a tty.
 *
 * Kees J. Bot - Feb 13 1993
 *  - putting out garbage.
 *  - added lastlog.
 *
 * Kees J. Bot - Feb 13 1993
 *  - supplementary groups.
 *
 * Kees J. Bot - Jan 3 1996
 *  - ported back to standard Minix.
 */

#define _MINIX_SOURCE
#define _POSIX_C_SOURCE	2

#include <sys/types.h>
#include <ttyent.h>
#include <sys/stat.h>
#include <errno.h>
#include <fcntl.h>
#include <pwd.h>
#include <grp.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <utmp.h>
#include <time.h>
#include <sys/utsname.h>
#include <minix/minlib.h>

char PATH_UTMP[] = "/etc/utmp";			/* current logins */
char PATH_WTMP[] = "/usr/adm/wtmp";		/* login/logout history */
char PATH_LASTLOG[] = "/usr/adm/lastlog";	/* last login history */
char PATH_MOTD[] = "/etc/motd";			/* message of the day */

#define TTY_GID		4			/* group ID of ttys */

#define EXTRA_ENV	6

/* Crude indication of a tty being physically secure: */
#define securetty(dev)		((unsigned) ((dev) - 0x0400) < (unsigned) 8)

int time_out;
char *hostname;
char user[32];
char logname[35];
char home[128];
char shell[128];
char term[128];
char **env;
extern char **environ;

_PROTOTYPE(int main, (int argc, char **argv));
_PROTOTYPE(void wtmp, (char *user, int uid));
_PROTOTYPE(void show_file, (char *nam));
_PROTOTYPE(void Time_out, (int dummy));
_PROTOTYPE(void usage, (void));
_PROTOTYPE(void add2env, (char **env, char *entry, int replace));

void wtmp(user, uid)
char *user;			/* user name */
int uid;			/* user id */
{
  /* Make entries in /usr/adm/wtmp and /etc/utmp. */
  struct utmp entry;
  register int fd= -1;
  int lineno;
  int err = 0;
  char *what;

  /* First, read the current UTMP entry. we need some of its
   * parameters! (like PID, ID etc...).
   */
  what= "ttyslot()";
  lineno= ttyslot();
  if (lineno == 0) err= errno;	/* ttyslot failed */

  if (err == 0 && (fd = open(what = PATH_UTMP, O_RDONLY)) < 0) {
  	if (errno == ENOENT) return;
  	err= errno;
  }
  if (err == 0 && lseek(fd, (off_t) lineno * sizeof(entry), SEEK_SET) < 0)
  	err= errno;
  if (err == 0 && read(fd, (char *) &entry, sizeof(entry)) != sizeof(entry))
  	err= errno;
  if (fd >= 0) close(fd);

  /* Enter new fields. */
  strncpy(entry.ut_user, user, sizeof(entry.ut_user));
  if (hostname) strncpy(entry.ut_host, hostname, sizeof(entry.ut_host));

  if (entry.ut_pid == 0) entry.ut_pid = getpid();

  entry.ut_type = USER_PROCESS;		/* we are past login... */
  time(&entry.ut_time);

  /* Write a WTMP record. */
  if (err == 0) {
  	if ((fd = open(what = PATH_WTMP, O_WRONLY|O_APPEND)) < 0) {
  		if (errno != ENOENT) err= errno;
	} else {
		if (write(fd, (char *) &entry, sizeof(entry)) < 0) err= errno;
		close(fd);
	}
  }

  /* Rewrite the UTMP entry. */
  if (err == 0 && (fd = open(what = PATH_UTMP, O_WRONLY)) < 0)
	err= errno;
  if (err == 0 && lseek(fd, (off_t) lineno * sizeof(entry), SEEK_SET) < 0)
	err= errno;
  if (err == 0 && write(fd, (char *) &entry, sizeof(entry)) < 0)
	err= errno;
  if (fd >= 0) close(fd);

  /* Write the LASTLOG entry. */
  if (err == 0 && (fd = open(what = PATH_LASTLOG, O_WRONLY)) < 0) {
	if (errno == ENOENT) return;
	err= errno;
  }
  if (err == 0 && lseek(fd, (off_t) uid * sizeof(entry), SEEK_SET) < 0)
	err= errno;
  if (err == 0 && write(fd, (char *) &entry, sizeof(entry)) < 0)
	err= errno;
  if (fd >= 0) close(fd);

  if (err != 0) {
  	fprintf(stderr, "login: %s: %s\n", what, strerror(err));
  	return;
  }
}


void show_file(nam)
char *nam;
{
/* Read a textfile and show it on the desired terminal. */
  register int fd, len;
  char buf[80];

  if ((fd = open(nam, O_RDONLY)) > 0) {
	len = 1;
	while (len > 0) {
		len = read(fd, buf, 80);
		write(1, buf, len);
	}
	close(fd);
  }
}


int main(argc, argv)
int argc;
char *argv[];
{
  char name[30];
  char *password, *cryptedpwd;
  char *tty_name, *p;
  int n, ap, check_pw, bad, secure, i, envsiz, do_banner;
  struct passwd *pwd;
  char *bp, *argx[8], **ep;	/* pw_shell arguments */
  char argx0[64];		/* argv[0] of the shell */
  char *sh = "/bin/sh";		/* sh/pw_shell field value */
  char *initialname;
  int c, b_flag, f_flag, p_flag;
  char *h_arg;
  int authorized, preserv_env;
  struct ttyent *ttyp;
  struct stat ttystat;
  struct sigaction sa;
  struct utsname uts;

  /* Don't let QUIT dump core. */
  sigemptyset(&sa.sa_mask);
  sa.sa_flags = 0;
  sa.sa_handler = exit;
  sigaction(SIGQUIT, &sa, NULL);

  /* Parse options.  */
  b_flag= 0;
  f_flag= 0;
  p_flag= 0;
  h_arg= NULL;
  while ((c= getopt(argc, argv, "?bfh:p")) != -1)
  {
	switch(c)
	{
	case 'b': b_flag= 1;	break;
	case 'f': f_flag= 1;	break;
	case 'h':
		if (h_arg)
			usage();
		if (getuid() == 0)
			h_arg= optarg;
		break;
	case 'p': p_flag= 1;	break;
	case '?':
		usage();
	default:
		fprintf(stderr, "login: getopt failed: '%c'\n", c);
		exit(1);
	}
  }
  if (optind < argc)
	initialname= argv[optind++];
  else
	initialname= NULL;
  if (optind != argc)
	usage();

  authorized= f_flag;
  hostname= h_arg;
  preserv_env= p_flag;
  do_banner= b_flag;

  /* Look up /dev/tty number. */
  tty_name= ttyname(0);
  if (tty_name == NULL)
  {
	write(1, "Unable to lookup tty name\n", 26);
	exit(1);
  }

  if (do_banner)
  {
	uname(&uts);
	write(1, "\n", 1);
	write(1, uts.sysname, strlen(uts.sysname));
	write(1, "/", 1);
	write(1, uts.machine, strlen(uts.machine));
	write(1, " Release ", 9);
	write(1, uts.release, strlen(uts.release));
	write(1, " Version ", 9);
	write(1, uts.version, strlen(uts.version));
	write(1, " (", 2);
	p= strrchr(tty_name, '/');
	if (!p)
		p= tty_name;
	else
		p++;
	write(1, p, strlen(p));
	write(1, ")\n\n", 3);
	write(1, uts.nodename, strlen(uts.nodename));
	write(1, " ", 1);
  }

  /* Get login name and passwd. */
  for (;;initialname= NULL) {
	if (initialname)
		strcpy(name, initialname);
	else {
		do {
			write(1, "login: ", 7);
			n = read(0, name, 30);
			if (n == 0) exit(1);
			if (n < 0)
			{
				if (errno != EINTR)
					fprintf(stderr,
						"login: read failed: %s\n",
							strerror(errno));
				exit(1);
			}
		} while (n < 2);
		name[n - 1] = 0;
	}

	/* Start timer running. */
	time_out = 0;
	sa.sa_handler = Time_out;
	sigaction(SIGALRM, &sa, NULL);
	alarm(60);


	/* Look up login/passwd. */
	pwd = getpwnam(name);

	check_pw = 1;			/* default is check password. */

	/* For now, only console is secure. */
	secure = fstat(0, &ttystat) == 0 && securetty(ttystat.st_rdev);

	if (pwd && authorized && initialname
			&& (pwd->pw_uid == getuid() || getuid() == 0)) {
		check_pw= 0;		/* Don't ask a password for
					 * pre-authorized users.
					 */
	} else
	if (pwd && secure && strcmp(crypt("", pwd->pw_passwd),
						pwd->pw_passwd) == 0) {
		check_pw= 0;		/* empty password */
	}

	if (check_pw) {
		password = getpass("Password:");

		if (time_out) exit(1);

		bad = 0;
		if (!pwd) bad = 1;
		if (!password) { password = ""; bad = 1; }
		if (!secure && pwd && strcmp(crypt("", pwd->pw_passwd),
					pwd->pw_passwd) == 0) bad = 1;

		cryptedpwd = bad ? "*" : pwd->pw_passwd;

		if (strcmp(crypt(password, cryptedpwd), cryptedpwd) != 0) {
			write(1, "Login incorrect\n", 16);
			continue;
		}
	}
	/* Check if the system is going down  */
	if (access("/etc/nologin", 0) == 0 && strcmp(name, "root") != 0) {
		write(1, "System going down\n\n", 19);
		continue;
	}

	/* Stop timer. */
	alarm(0);

	/* Write login record to /usr/adm/wtmp and /etc/utmp */
	wtmp(name, pwd->pw_uid);

	/* Create the argv[] array from the pw_shell field. */
	ap = 0;
	argx[ap++] = argx0;	/* "-sh" most likely */
	if (pwd->pw_shell[0]) {
		sh = pwd->pw_shell;
		bp = sh;
		while (*bp) {
			while (*bp && *bp != ' ' && *bp != '\t') bp++;
			if (*bp == ' ' || *bp == '\t') {
				*bp++ = '\0';	/* mark end of string */
				argx[ap++] = bp;
			}
		}
	} else
	argx[ap] = NULL;
	strcpy(argx0, "-");	/* most shells need it for their .profile */
	if ((bp= strrchr(sh, '/')) == NULL) bp = sh; else bp++;
	strncat(argx0, bp, sizeof(argx0) - 2);

	/* Set the environment */
	if (p_flag)
	{
		for (ep= environ; *ep; ep++)
			;
	}
	else
		ep= environ;

	envsiz= ep-environ;
	env= calloc(envsiz + EXTRA_ENV, sizeof(*env));
	if (env == NULL)
	{
		fprintf(stderr, "login: out of memory\n");
		exit(1);
	}
	for (i= 0; i<envsiz; i++)
		env[i]= environ[i];

	strcpy(user, "USER=");
	strcat(user, name);
	add2env(env, user, 1);
	strcpy(logname, "LOGNAME=");
	strcat(logname, name);
	add2env(env, logname, 1);
	strcpy(home, "HOME=");
	strcat(home, pwd->pw_dir);
	add2env(env, home, 1);
	strcpy(shell, "SHELL=");
	strcat(shell, sh);
	add2env(env, shell, 1);
	if ((ttyp = getttynam(tty_name + 5)) != NULL) {
		strcpy(term, "TERM=");
		strcat(term, ttyp->ty_type);
		add2env(env, term, 0);
	}

	/* Show the message-of-the-day. */
	show_file(PATH_MOTD);

	/* Assign the terminal to this user. */
	chown(tty_name, pwd->pw_uid, TTY_GID);
	chmod(tty_name, 0620);

	/* Change id. */
#if __minix_vmd
	initgroups(pwd->pw_name, pwd->pw_gid);
#endif
	setgid(pwd->pw_gid);
	setuid(pwd->pw_uid);

	/* cd $HOME */
	chdir(pwd->pw_dir);

	/* Reset signals to default values. */
	sa.sa_handler = SIG_DFL;
	for (n = 1; n <= _NSIG; ++n) sigaction(n, &sa, NULL);

	/* Execute the user's shell. */
	execve(sh, argx, env);

	if (pwd->pw_gid == 0) {
		/* Privileged user gets /bin/sh in times of crisis. */
		sh= "/bin/sh";
		argx[0]= "-sh";
		strcpy(shell, "SHELL=");
		strcat(shell, sh);
		execve(sh, argx, env);
	}
	fprintf(stderr, "login: can't execute %s: %s\n", sh, strerror(errno));
	exit(1);
  }
  return(0);
}


void Time_out(dummy)
int dummy; /* to keep the compiler happy */
{
   write(2, "\r\nLogin timed out after 60 seconds\r\n", 36);
   time_out = 1;
}

void usage()
{
	fprintf(stderr,
		"Usage: login [-h hostname] [-b] [-f] [-p] [username]\n");
	exit(1);
}

void add2env(env, entry, replace)
char **env;
char *entry;
int replace;
{
/* Replace an environment variable with entry or add entry if the environment
 * variable doesn't exit yet. 
 */
	char *cp;
	int keylen;

	cp= strchr(entry, '=');
	keylen= cp-entry+1;

	for(; *env; env++)
	{
		if (strncmp(*env, entry, keylen) == 0) {
			if (!replace) return;		/* Don't replace */
			break;
		}
	}
	*env= entry;
}

/*
 * $PchId: login.c,v 1.6 2001/07/31 14:23:28 philip Exp $
 */
