/* This process is the father (mother) of all Minix user processes.  When
 * Minix comes up, this is process number 2, and has a pid of 1.  It
 * executes the /etc/rc shell file, and then reads the /etc/ttytab file to
 * determine which terminals need a login process.
 *
 * If the files /usr/adm/wtmp and /etc/utmp exist and are writable, init
 * (with help from login) will maintain login accounting.  Sending a
 * signal 1 (SIGHUP) to init will cause it to rescan /etc/ttytab and start
 * up new shell processes if necessary.  It will not, however, kill off
 * login processes for lines that have been turned off; do this manually.
 * Signal 15 (SIGTERM) makes init stop spawning new processes, this is
 * used by shutdown and friends when they are about to close the system
 * down.
 */

#include <minix/type.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <sys/stat.h>
#include <sys/svrctl.h>
#include <ttyent.h>
#include <errno.h>
#include <fcntl.h>
#include <limits.h>
#include <signal.h>
#include <string.h>
#include <time.h>
#include <stdlib.h>
#include <unistd.h>
#include <utmp.h>
#include <minix/reboot.h>

/* Different ttyent structure. */
struct ttyent TT_REBOOT = { "console", "shutdown -d now CTRL-ALT_DEL", "-"};

char PATH_UTMP[] = "/etc/utmp";		/* current logins */
char PATH_WTMP[] = "/usr/adm/wtmp";	/* login/logout history */
char PATH_ROOT_WTMP[] = "/etc/wtmp";	/* wtmp for system up/down events */

#define PIDSLOTS	32		/* first this many ttys can be on */

struct slotent {
  int errct;			/* error count */
  pid_t pid;			/* pid of login process for this tty line */
};

#define ERRCT_DISABLE	10	/* disable after this many errors */
#define NO_PID	0		/* pid value indicating no process */

struct slotent slots[PIDSLOTS];	/* init table of ttys and pids */

int gothup = 0;			/* flag, showing signal 1 was received */
int gotabrt = 0;		/* flag, showing signal 6 was received */
int spawn = 1;			/* flag, spawn processes only when set */

void tell(int fd, char *s);
void report(int fd, char *label);
void wtmp(int type, int linenr, char *line, pid_t pid);
void startup(int linenr, struct ttyent *ttyp);
int execute(char **cmd);
char **construct_argv(char *cmd);
void onhup(int sig);
void onterm(int sig);
void onabrt(int sig);

int main(void)
{
  pid_t pid;			/* pid of child process */
  int fd;			/* generally useful */
  int linenr;			/* loop variable */
  int check;			/* check if a new process must be spawned */
  int sn;			/* signal number */
  struct slotent *slotp;	/* slots[] pointer */
  struct ttyent *ttyp;		/* ttytab entry */
  struct sigaction sa;
  struct stat stb;

#define OPENFDS						\
  if (fstat(0, &stb) < 0) {				\
	/* Open standard input, output & error. */	\
	(void) open("/dev/null", O_RDONLY);		\
	(void) open("/dev/log", O_WRONLY);		\
	dup(1);						\
  }

  sigemptyset(&sa.sa_mask);
  sa.sa_flags = 0;

  /* Default: Ignore every signal (except those that follow). */
  sa.sa_handler = SIG_IGN;
  for (sn = 1; sn < _NSIG; sn++) {
      sigaction(sn, &sa, NULL);
  }

  /* Hangup: Reexamine /etc/ttytab for newly enabled terminal lines. */
  sa.sa_handler = onhup;
  sigaction(SIGHUP, &sa, NULL);

  /* Terminate: Stop spawning login processes, shutdown is near. */
  sa.sa_handler = onterm;
  sigaction(SIGTERM, &sa, NULL);

  /* Abort: Sent by the kernel on CTRL-ALT-DEL; shut the system down. */
  sa.sa_handler = onabrt;
  sigaction(SIGABRT, &sa, NULL);

  /* Execute the /etc/rc file. */
  if ((pid = fork()) != 0) {
	/* Parent just waits. */
	while (wait(NULL) != pid) {
		if (gotabrt) reboot(RBT_HALT);
	}
  } else {
#if ! SYS_GETKENV
	struct sysgetenv sysgetenv;
#endif
	char bootopts[16];
	static char *rc_command[] = { "sh", "/etc/rc", NULL, NULL, NULL };
	char **rcp = rc_command + 2;

	/* Get the boot options from the boot environment. */
	sysgetenv.key = "bootopts";
	sysgetenv.keylen = 8+1;
	sysgetenv.val = bootopts;
	sysgetenv.vallen = sizeof(bootopts);
	if (svrctl(PMGETPARAM, &sysgetenv) == 0) *rcp++ = bootopts;
	*rcp = "start";

	execute(rc_command);
	report(2, "sh /etc/rc");
	_exit(1);	/* impossible, we hope */
  }

  OPENFDS;

  /* Clear /etc/utmp if it exists. */
  if ((fd = open(PATH_UTMP, O_WRONLY | O_TRUNC)) >= 0) close(fd);

  /* Log system reboot. */
  wtmp(BOOT_TIME, 0, NULL, 0);

  /* Main loop. If login processes have already been started up, wait for one
   * to terminate, or for a HUP signal to arrive. Start up new login processes
   * for all ttys which don't have them. Note that wait() also returns when
   * somebody's orphan dies, in which case ignore it.  If the TERM signal is
   * sent then stop spawning processes, shutdown time is near.
   */

  check = 1;
  while (1) {
	while ((pid = waitpid(-1, NULL, check ? WNOHANG : 0)) > 0) {
		/* Search to see which line terminated. */
		for (linenr = 0; linenr < PIDSLOTS; linenr++) {
			slotp = &slots[linenr];
			if (slotp->pid == pid) {
				/* Record process exiting. */
				wtmp(DEAD_PROCESS, linenr, NULL, pid);
				slotp->pid = NO_PID;
				check = 1;
			}
		}
	}

	/* If a signal 1 (SIGHUP) is received, simply reset error counts. */
	if (gothup) {
		gothup = 0;
		for (linenr = 0; linenr < PIDSLOTS; linenr++) {
			slots[linenr].errct = 0;
		}
		check = 1;
	}

	/* Shut down on signal 6 (SIGABRT). */
	if (gotabrt) {
		gotabrt = 0;
		startup(0, &TT_REBOOT);
	}

	if (spawn && check) {
		/* See which lines need a login process started up. */
		for (linenr = 0; linenr < PIDSLOTS; linenr++) {
			slotp = &slots[linenr];
			if ((ttyp = getttyent()) == NULL) break;

			if (ttyp->ty_getty != NULL
				/* ty_getty is a string, and TTY_ON is
				 * the way to check for enabled ternimanls. */
				&& (ttyp->ty_status & TTY_ON)
				&& slotp->pid == NO_PID
				&& slotp->errct < ERRCT_DISABLE)
			{
				startup(linenr, ttyp);
			}
		}
		endttyent();
	}
	check = 0;
  }
}

void onhup(int sig)
{
  gothup = 1;
  spawn = 1;
}

void onterm(int sig)
{
  spawn = 0;
}

void onabrt(int sig)
{
  static int count = 0;

  if (++count == 2) reboot(RBT_HALT);
  gotabrt = 1;
}

void startup(int linenr, struct ttyent *ttyp)
{
  /* Fork off a process for the indicated line. */

  struct slotent *slotp;		/* pointer to ttyslot */
  pid_t pid;				/* new pid */
  int err[2];				/* error reporting pipe */
  char line[32];			/* tty device name */
  char **ty_getty_argv;

  slotp = &slots[linenr];

  /* Error channel for between fork and exec. */
  if (pipe(err) < 0) err[0] = err[1] = -1;

  if ((pid = fork()) == -1 ) {
	report(2, "fork()");
	sleep(10);
	return;
  }

  if (pid == 0) {
	/* Child */
	close(err[0]);
	fcntl(err[1], F_SETFD, fcntl(err[1], F_GETFD) | FD_CLOEXEC);

	/* A new session. */
	setsid();

	/* Construct device name. */
	strcpy(line, "/dev/");
	strncat(line, ttyp->ty_name, sizeof(line) - 6);

	/* Open the line for standard input and output. */
	close(0);
	close(1);
	if (open(line, O_RDWR) < 0 || dup(0) < 0) {
		write(err[1], &errno, sizeof(errno));
		_exit(1);
	}

	/* Redirect standard error too. */
	dup2(0, 2);

	/* Construct argv for execute() */
	ty_getty_argv = construct_argv(ttyp->ty_getty);
	if (ty_getty_argv == NULL)
		report(2, "construct_argv");

	/* Execute the getty process. */
	execute(ty_getty_argv);

	/* Oops, disaster strikes. */
	fcntl(2, F_SETFL, fcntl(2, F_GETFL) | O_NONBLOCK);
	if (linenr != 0) report(2, ty_getty_argv[0]);
	write(err[1], &errno, sizeof(errno));
	_exit(1);
  }

  /* Parent */
  if (ttyp != &TT_REBOOT) slotp->pid = pid;

  close(err[1]);
  if (read(err[0], &errno, sizeof(errno)) != 0) {
	/* If an errno value goes down the error pipe: Problems. */

	switch (errno) {
	case ENOENT:
	case ENODEV:
	case ENXIO:
		/* Device nonexistent, no driver, or no minor device. */
		slotp->errct = ERRCT_DISABLE;
		close(err[0]);
		return;
	case 0:
		/* Error already reported. */
		break;
	default:
		/* Any other error on the line. */
		report(2, ttyp->ty_name);
	}
	close(err[0]);

	if (++slotp->errct >= ERRCT_DISABLE) {
		tell(2, "init: ");
		tell(2, ttyp->ty_name);
		tell(2, ": excessive errors, shutting down\n");
	} else {
		sleep(5);
	}
	return;
  }
  close(err[0]);

  if (ttyp != &TT_REBOOT) wtmp(LOGIN_PROCESS, linenr, ttyp->ty_name, pid);
  slotp->errct = 0;
}

int execute(char **cmd)
{
  /* Execute a command with a path search along /sbin:/bin:/usr/sbin:/usr/bin.
   */
  static char *nullenv[] = { NULL };
  char command[128];
  char *path[] = { "/sbin", "/bin", "/usr/sbin", "/usr/bin" };
  int i;

  if (cmd[0][0] == '/') {
	/* A full path. */
	return execve(cmd[0], cmd, nullenv);
  }

  /* Path search. */
  for (i = 0; i < 4; i++) {
	if (strlen(path[i]) + 1 + strlen(cmd[0]) + 1 > sizeof(command)) {
		errno= ENAMETOOLONG;
		return -1;
	}
	strcpy(command, path[i]);
	strcat(command, "/");
	strcat(command, cmd[0]);
	execve(command, cmd, nullenv);
	if (errno != ENOENT) break;
  }
  return -1;
}

void wtmp(type, linenr, line, pid)
int type;			/* type of entry */
int linenr;			/* line number in ttytab */
char *line;			/* tty name (only good on login) */
pid_t pid;			/* pid of process */
{
/* Log an event into the UTMP and WTMP files. */

  struct utmp utmp;		/* UTMP/WTMP User Accounting */
  int fd;

  /* Clear the utmp record. */
  memset((void *) &utmp, 0, sizeof(utmp));

  /* Fill in utmp. */
  switch (type) {
  case BOOT_TIME:
	/* Make a special reboot record. */
	strcpy(utmp.ut_name, "reboot");
	strcpy(utmp.ut_line, "~");
	break;

  case LOGIN_PROCESS:
  	/* A new login, fill in line name. */
	strncpy(utmp.ut_line, line, sizeof(utmp.ut_line));
	break;

  case DEAD_PROCESS:
	/* A logout.  Use the current utmp entry, but make sure it is a
	 * user process exiting, and not getty or login giving up.
	 */
	if ((fd = open(PATH_UTMP, O_RDONLY)) < 0) {
		if (errno != ENOENT) report(2, PATH_UTMP);
		return;
	}
	if (lseek(fd, (off_t) (linenr+1) * sizeof(utmp), SEEK_SET) == -1
		|| read(fd, &utmp, sizeof(utmp)) == -1
	) {
		report(2, PATH_UTMP);
		close(fd);
		return;
	}
	close(fd);
	if (utmp.ut_type != USER_PROCESS) return;
	strncpy(utmp.ut_name, "", sizeof(utmp.ut_name));
	break;
  }

  /* Finish new utmp entry. */
  utmp.ut_pid = pid;
  utmp.ut_type = type;
  utmp.ut_time = time((time_t *) 0);

  switch (type) {
  case LOGIN_PROCESS:
  case DEAD_PROCESS:
	/* Write new entry to utmp. */
	if ((fd = open(PATH_UTMP, O_WRONLY)) < 0
		|| lseek(fd, (off_t) (linenr+1) * sizeof(utmp), SEEK_SET) == -1
		|| write(fd, &utmp, sizeof(utmp)) == -1
	) {
		if (errno != ENOENT) report(2, PATH_UTMP);
	}
	if (fd != -1) close(fd);
	break;
  }

  switch (type) {
  case BOOT_TIME:
	/* Add new root wtmp entry. */
	if ((fd = open(PATH_ROOT_WTMP, O_WRONLY | O_APPEND)) < 0
		  || write(fd, &utmp, sizeof(utmp)) == -1
	) {
		if (errno != ENOENT) report(2, PATH_ROOT_WTMP);
	}
	if (fd != -1) close(fd);
	/* fall-through */
  case DEAD_PROCESS:
	/* Add new wtmp entry. */
	if ((fd = open(PATH_WTMP, O_WRONLY | O_APPEND)) < 0
		  || write(fd, &utmp, sizeof(utmp)) == -1
	) {
		if (errno != ENOENT) report(2, PATH_WTMP);
	}
	if (fd != -1) close(fd);
	break;
  }
}

char **
construct_argv(char *cmd)
{
	int argc = 0;
	static const char sep[] = " \t";
	char **argv = malloc(((strlen(cmd) + 1) / 2 + 1) * sizeof (char *));

	if (argv == NULL)
		return NULL;

	if ((argv[argc++] = strtok(cmd, sep)) == 0) {
		free(argv);
		return NULL;
	}
	while ((argv[argc++] = strtok(NULL, sep)) != NULL)
		continue;
	return argv;
}

void tell(fd, s)
int fd;
char *s;
{
	write(fd, s, strlen(s));
}

void report(fd, label)
int fd;
char *label;
{
	int err = errno;

	tell(fd, "init: ");
	tell(fd, label);
	tell(fd, ": ");
	tell(fd, strerror(err));
	tell(fd, "\n");
	errno= err;
}
