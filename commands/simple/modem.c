/* modem - Put modem into DIALIN or DIALOUT mode.	Author: F. van Kempen */

/* Exit:	0	OK, suspended/restarted GETTY
 *		1	UNIX error
 *		2	Process busy
 * Version:	1.3 	12/30/89
 *
 * Author:	F. van Kempen, MicroWalt Corporation
 *
 * All fancy stuff removed, see getty.c.	Kees J. Bot.
 *
 */

#include <stdio.h>
#include <stdlib.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/wait.h>
#include <fcntl.h>
#include <signal.h>
#include <string.h>
#include <unistd.h>
#include <utmp.h>
#include <errno.h>

char PATH_UTMP[] = "/etc/utmp";			/* current logins */

_PROTOTYPE(void usage , (void));
_PROTOTYPE(int main , (int argc , char *argv []));
_PROTOTYPE(void sendcodes , (char *tty, char *codes));


void usage()
{
  fprintf(stderr,
"Usage: modem [-sio] [-I in-codes] [-O out-codes] line [command args ...]\n");
  exit(1);
}


main(argc, argv)
int argc;
char *argv[];
{
  struct utmp entry;
  char devtty[1024], *tty;
  char **command;
  int ex_code = 0;
  int fd, i, slot, getty;
  struct stat st;
  enum { TOGGLE, DIALIN, DIALOUT } mode= TOGGLE;
  int silent = 0;
  _PROTOTYPE(void (*hsig), (int));
  _PROTOTYPE(void (*isig), (int));
  _PROTOTYPE(void (*qsig), (int));
  _PROTOTYPE(void (*tsig), (int));
  pid_t pid;
  int r, status;
  uid_t uid = getuid();
  gid_t gid = getgid();
  char *in_codes, *out_codes;

  i = 1;
  while (i < argc && argv[i][0] == '-') {
	char *opt = argv[i++] + 1;

	if (opt[0] == '-' && opt[1] == 0) break;

	while (*opt != 0) {
		switch (*opt++) {
		    case 's':	/* silent mode */
			silent = 1;
			break;
		    case 'i':	/* DIAL-IN mode: suspend GETTY */
			mode = DIALIN;
			break;
		    case 'o':	/* DIAL-OUT mode: restart GETTY */
			mode = DIALOUT;
			break;
		    case 'I':	/* code to switch modem to dial-in */
			if (*opt == 0) {
				if (i == argc) usage();
				opt = argv[i++];
			}
			in_codes = opt;
			opt = "";
			break;
		    case 'O':	/* code to switch modem to dial-out */
			if (*opt == 0) {
				if (i == argc) usage();
				opt = argv[i++];
			}
			out_codes = opt;
			opt = "";
			break;
		    default:
			usage();
		}
	}
  }

  if (i == argc) usage();
  tty = argv[i++];		/* Modem line */

  if (mode != TOGGLE && i != argc) usage();
  command = argv + i;		/* Command to execute (if any). */

  if (strchr(tty, '/') == NULL) {
	strcpy(devtty, "/dev/");
	strncat(devtty, tty, 1024 - 6);
	tty = devtty;
  }

  if (stat(tty, &st) < 0) {
	fprintf(stderr, "modem: %s: %s\n", tty, strerror(errno));
	exit(1);
  }

  if (!S_ISCHR(st.st_mode)) {
	fprintf(stderr, "%s is not a tty\n", tty);
	exit(1);
  }

  /* Find the utmp slot number for the line. */
  if ((fd= open(tty, O_RDONLY)) < 0 || (slot= fttyslot(fd)) == 0) {
	fprintf(stderr, "modem: %s: %s\n", tty, strerror(errno));
	exit(1);
  }
  close(fd);

  /* Read the UTMP file to find out the PID and STATUS of the GETTY. */
  entry.ut_type= 0;
  if ((fd = open(PATH_UTMP, O_RDONLY)) < 0
	|| lseek(fd, (off_t) slot * sizeof(entry), SEEK_SET) < 0
	|| read(fd, &entry, sizeof(entry)) < 0
  ) {
	fprintf(stderr, "modem: cannot read UTMP !\n");
	exit(1);
  }
  close(fd);

  hsig= signal(SIGHUP, SIG_IGN);
  isig= signal(SIGINT, SIG_IGN);
  qsig= signal(SIGQUIT, SIG_IGN);
  tsig= signal(SIGTERM, SIG_IGN);

  /* Process the terminal entry if we got one. */
  switch (entry.ut_type) {
  case LOGIN_PROCESS:		/* getty waiting for a call */
	getty = 1;
	break;
  case USER_PROCESS:		/* login or user-shell */
	if (!silent) fprintf(stderr, "modem: line is busy.\n");
	exit(2);
	break;
  default:
	getty = 0;
  }

  for (i = (mode == TOGGLE) ? 0 : 1; i < 2; i++) {
	/* Now perform the desired action (DIALIN or DIALOUT). */
	switch (mode) {
	case DIALOUT:
	case TOGGLE:
		if (getty) kill(entry.ut_pid, SIGUSR1);  /* suspend getty */
		chown(tty, uid, st.st_gid);	/* give line to user */
		chmod(tty, 0600);
		if (out_codes != NULL) sendcodes(tty, out_codes);
		if (!silent) printf("modem on %s set for dialout.\n", tty);
		break;
	case DIALIN:
		if (in_codes != NULL) sendcodes(tty, in_codes);
		chown(tty, 0, st.st_gid);		/* revoke access */
		chmod(tty, 0600);
		if (getty) kill(entry.ut_pid, SIGUSR2);	/* restart getty */
		if (!silent) printf("modem on %s set for dialin.\n", tty);
	}
	if (mode == TOGGLE) {
		/* Start the command to run */
		pid_t pid;
		int status;

		switch ((pid = fork())) {
		case -1:
			fprintf(stderr, "modem: fork(): %s\n", strerror(errno));
			ex_code= 1;
			break;
		case 0:
			setgid(gid);
			setuid(uid);
			(void) signal(SIGHUP, hsig);
			(void) signal(SIGINT, isig);
			(void) signal(SIGQUIT, qsig);
			(void) signal(SIGTERM, tsig);
			execvp(command[0], command);
			fprintf(stderr, "modem: %s: %s\n",
					command[0], strerror(errno));
			_exit(127);
		default:
			while ((r= wait(&status)) != pid) {
				if (r == -1 && errno != EINTR) break;
			}
			if (r == -1 || status != 0) ex_code = 1;
		}
		mode = DIALIN;
	}
  }
  exit(ex_code);
}

void sendcodes(tty, codes)
char *tty, *codes;
{
	int fd;
	int c;
	char buf[1024], *bp = buf;

	if ((fd = open(tty, O_RDWR|O_NONBLOCK)) < 0) {
		fprintf(stderr, "modem: can't send codes to %s: %s\n",
			tty, strerror(errno));
		return;
	}
	while ((c = *codes++) != 0) {
fprintf(stderr, "%d\n", __LINE__);
		if (c == '\\') {
			if ((c = *codes++) == 0) break;
			if (c == 'r') c= '\r';
			if (c == 'n') c= '\n';
		}
		*bp++ = c;
		if (bp == buf + sizeof(buf) || c == '\r' || c == '\n') {
fprintf(stderr, "%d\n", __LINE__);
			write(fd, buf, bp - buf);
fprintf(stderr, "%d\n", __LINE__);
			do {sleep(1);
fprintf(stderr, "%d\n", __LINE__);
			fprintf(stderr, "%d\n", read(fd, buf, sizeof(buf)));
			}while (read(fd, buf, sizeof(buf)) > 0);
fprintf(stderr, "%d\n", __LINE__);
			bp = buf;
		}
	}
	if (bp > buf) {
fprintf(stderr, "%d\n", __LINE__);
		write(fd, buf, bp - buf);
fprintf(stderr, "%d\n", __LINE__);
		do sleep(1); while (read(fd, buf, sizeof(buf)) > 0);
fprintf(stderr, "%d\n", __LINE__);
	}
	close(fd);
}
