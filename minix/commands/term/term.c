/* term - terminal simulator		Author: Andy Tanenbaum */

/* This program allows the user to turn a MINIX system into a dumb
 * terminal to communicate with a remote computer through one of the ttys.
 * It forks into two processes.  The parent sits in a tight loop copying
 * from stdin to the tty.  The child sits in a tight loop copying from
 * the tty to stdout.
 *
 * 2 Sept 88 BDE (Bruce D. Evans): Massive changes to make current settings the
 * default, allow any file as the "tty", support fancy baud rates and remove
 * references to and dependencies on modems and keyboards, so (e.g.)
 * a local login on /dev/tty1 can do an external login on /dev/tty2.
 *
 * 3 Sept 88 BDE: Split parent again to main process copies from stdin to a
 * pipe which is copied to the tty.  This stops a blocked write to the
 * tty from hanging the program.
 *
 * 11 Oct 88 BDE: Cleaned up baud rates and parity stripping.
 *
 * 09 Oct 90 MAT (Michael A. Temari): Fixed bug where terminal isn't reset
 * if an error occurs.
 *
 * Nov 90 BDE: Don't broadcast kill(0, SIGINT) since two or more of these
 * in a row will kill the parent shell.
 *
 * 19 Oct 89 RW (Ralf Wenk): Adapted to MINIX ST 1.1 + RS232 driver. Split
 * error into error_n and error. Added resetting of the terminal settings
 * in error.
 *
 * 24 Nov 90 RW: Adapted to MINIX ST 1.5.10.2. Forked processes are now
 * doing an exec to get a better performance. This idea is stolen from
 * a terminal program written by Felix Croes.
 *
 * 01 May 91 RW: Merged the MINIX ST patches with Andys current version.
 * Most of the 19 Oct 89 patches are deleted because they are already there.
 *
 * 10 Mar 96 KJB: Termios adaption, cleanup, command key interface.
 *
 * 27 Nov 96 KJB: Add -c flag that binds commands to keys.
 *
 * Example usage:
 *	term			: baud, bits/char, parity from /dev/tty1
 *	term 9600 7 even	: 9600 baud, 7 bits/char, even parity
 *	term odd 300 7		:  300 baud, 7 bits/char, odd parity
 *	term /dev/tty2		: use /dev/tty2 rather than /dev/tty1
 *				: Any argument starting with "/" is
 *				: taken as the communication device.
 *	term 8 57600 /dev/tty2 -atdt4441234	: if an argument begins with
 *						: - , the rest of that arg is
 *						: sent to the modem as a
 *						: dial string
 */

#include <sys/types.h>
#include <fcntl.h>
#include <termios.h>
#include <signal.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <stdarg.h>
#include <errno.h>
#include <sys/wait.h>
#include <sys/stat.h>

#define CHUNK 1024		/* how much to read at once */

char TERM_LINE[] = "/dev/modem";/* default serial port to use */

				/* device lock file */
char lockfile[] = "/usr/spool/locks/LK.iii.jjj.kkk";

char *commdev;			/* communications device a.k.a. "modem". */
int commfd;			/* open file no. for comm device */
struct termios tccomm;		/* terminal parameters for commfd */
struct termios tcstdin;		/* terminal parameters for stdin */
struct termios tcsavestdin;	/* saved terminal parameters for stdin */

/* Special key to get term's attention. */
#define HOTKEY	'\035'		/* CTRL-] */

struct param_s {
  char *pattern;
  unsigned value;
  enum { BAD, BITS, PARITY, SPEED } type;
} params[] = {
  { "5",	CS5,		BITS	},
  { "6",	CS6,		BITS	},
  { "7",	CS7,		BITS	},
  { "8",	CS8,		BITS	},

  { "even",	PARENB,		PARITY	},
  { "odd",	PARENB|PARODD,	PARITY	},

  { "50",	B50,		SPEED	},
  { "75",	B75,		SPEED	},
  { "110",	B110,		SPEED	},
  { "134",	B134,		SPEED	},
  { "200",	B200,		SPEED	},
  { "300",	B300,		SPEED	},
  { "600",	B600,		SPEED	},
  { "1200",	B1200,		SPEED	},
  { "1800",	B1800,		SPEED	},
  { "2400",	B2400,		SPEED	},
  { "4800",	B4800,		SPEED	},
  { "9600",	B9600,		SPEED	},
  { "19200",	B19200,		SPEED	},
  { "38400",	B38400,		SPEED	},
  { "57600",	B57600,		SPEED	},
  { "115200",	B115200,	SPEED	},
  { "",		0,		BAD	},	/* BAD type to end list */
};

#define NIL ((char *) NULL)		/* tell(fd, ..., NIL) */

int main(int argc, char *argv[]);
int isdialstr(char *arg);
void tell(int fd, ...);
void reader(int on);
void shell(char *cmd);
void lock_device(char *device);
void fatal(char *label);
void setnum(char *s, int n);
void set_uart(int argc, char *argv[], struct termios *tcp);
void set_raw(struct termios *tcp);
void quit(int code);

int main(argc, argv)
int argc;
char *argv[];
{
  int i;
  unsigned char key;
  int candial;

  for (i = 1; i < argc; ++i) {
	if (argv[i][0] == '/') {
		if (commdev != NULL) {
			tell(2, "term: too many communication devices\n", NIL);
			exit(1);
		}
		commdev = argv[i];
	}
  }
  if (commdev == NULL) commdev = TERM_LINE;

  /* Save tty attributes of the terminal. */
  if (tcgetattr(0, &tcsavestdin) < 0) {
	tell(2, "term: standard input is not a terminal\n", NIL);
	exit(1);
  }

  lock_device(commdev);

  commfd = open(commdev, O_RDWR);
  if (commfd < 0) {
	tell(2, "term: can't open ", commdev, ": ", strerror(errno), "\n", NIL);
	quit(1);
  }

  /* Compute RAW modes of terminal and modem. */
  if (tcgetattr(commfd, &tccomm) < 0) {
	tell(2, "term: ", commdev, " is not a terminal\n", NIL);
	quit(1);
  }
  signal(SIGINT, quit);
  signal(SIGTERM, quit);
  tcstdin = tcsavestdin;
  set_raw(&tcstdin);
  set_raw(&tccomm);
  set_uart(argc, argv, &tccomm);
  tcsetattr(0, TCSANOW, &tcstdin);
  tcsetattr(commfd, TCSANOW, &tccomm);

  /* Start a reader process to copy modem output to the screen. */
  reader(1);

  /* Welcome message. */
  tell(1, "Connected to ", commdev,
			", command key is CTRL-], type ^]? for help\r\n", NIL);

  /* Dial. */
  candial = 0;
  for (i = 1; i < argc; ++i) {
	if (!isdialstr(argv[i])) continue;
	tell(commfd, argv[i] + 1, "\r", NIL);
	candial = 1;
  }

  /* Main loop of the terminal simulator. */
  while (read(0, &key, 1) == 1) {
	if (key == HOTKEY) {
		/* Command key typed. */
		if (read(0, &key, 1) != 1) continue;

		switch (key) {
		default:
			/* Added command? */
			for (i = 1; i < argc; ++i) {
				char *arg = argv[i];

				if (arg[0] == '-' && arg[1] == 'c'
							&& arg[2] == key) {
					reader(0);
					tcsetattr(0, TCSANOW, &tcsavestdin);
					shell(arg+3);
					tcsetattr(0, TCSANOW, &tcstdin);
					reader(1);
					break;
				}
			}
			if (i < argc) break;

			/* Unrecognized command, print list. */
			tell(1, "\r\nTerm commands:\r\n",
				" ? - this help\r\n",
				candial ? " d - redial\r\n" : "",
				" s - subshell (e.g. for file transfer)\r\n",
				" h - hangup (+++ ATH)\r\n",
				" b - send a break\r\n",
				" q - exit term\r\n",
				NIL);
			for (i = 1; i < argc; ++i) {
				char *arg = argv[i];
				static char cmd[] = " x - ";

				if (arg[0] == '-' && arg[1] == 'c'
							&& arg[2] != 0) {
					cmd[1] = arg[2];
					tell(1, cmd, arg+3, "\r\n", NIL);
				}
			}
			tell(1, "^] - send a CTRL-]\r\n\n",
				NIL);
			break;
		case 'd':
			/* Redial by sending the dial commands again. */
			for (i = 1; i < argc; ++i) {
				if (!isdialstr(argv[i])) continue;
				tell(commfd, argv[i] + 1, "\r", NIL);
			}
			break;
		case 's':
			/* Subshell. */
			reader(0);
			tcsetattr(0, TCSANOW, &tcsavestdin);
			shell(NULL);
			tcsetattr(0, TCSANOW, &tcstdin);
			reader(1);
			break;
		case 'h':
			/* Hangup by using the +++ escape and ATH command. */
			sleep(2);
			tell(commfd, "+++", NIL);
			sleep(2);
			tell(commfd, "ATH\r", NIL);
			break;
		case 'b':
			/* Send a break. */
			tcsendbreak(commfd, 0);
			break;
		case 'q':
			/* Exit term. */
			quit(0);
		case HOTKEY:
			(void) write(commfd, &key, 1);
			break;
		}
	} else {
		/* Send keyboard input down the serial line. */
		if (write(commfd, &key, 1) != 1) break;
	}
  }
  tell(2, "term: nothing to copy from input to ", commdev, "?\r\n", NIL);
  quit(1);
}


int isdialstr(char *arg)
{
/* True iff arg is the start of a dial string, i.e. "-at...". */

  return (arg[0] == '-'
  	&& (arg[1] == 'a' || arg[1] == 'A')
  	&& (arg[2] == 't' || arg[2] == 'T'));
}


void tell(int fd, ...)
{
/* Write strings to file descriptor 'fd'. */
  va_list ap;
  char *s;

  va_start(ap, fd);
  while ((s = va_arg(ap, char *)) != NIL) write(fd, s, strlen(s));
  va_end(ap);
}


void reader(on)
int on;
{
/* Start or end a process that copies from the modem to the screen. */

  static pid_t pid;
  char buf[CHUNK];
  ssize_t n, m, r;

  if (!on) {
	/* End the reader process (if any). */
	if (pid == 0) return;
	kill(pid, SIGKILL);
	(void) waitpid(pid, (int *) NULL, 0);
	pid = 0;
	return;
  }

  /* Start a reader */
  pid = fork();
  if (pid < 0) {
	tell(2, "term: fork() failed: ", strerror(errno), "\r\n", NIL);
	quit(1);
  }
  if (pid == 0) {
	/* Child: Copy from the modem to the screen. */

	while ((n = read(commfd, buf, sizeof(buf))) > 0) {
		m = 0;
		while (m < n && (r = write(1, buf + m, n - m)) > 0) m += r;
	}
	tell(2, "term: nothing to copy from ", commdev, " to output?\r\n", NIL);
	kill(getppid(), SIGTERM);
	_exit(1);
  }
  /* One reader on the loose. */
}


void shell(char *cmd)
{
/* Invoke a subshell to allow one to run zmodem for instance.  Run sh -c 'cmd'
 * instead if 'cmd' non-null.
 */

  pid_t pid;
  char *shell, *sh0;
  void(*isav) (int);
  void(*qsav) (int);
  void(*tsav) (int);

  if (cmd == NULL) {
	tell(1, "\nExit the shell to return to term, ",
		commdev, " is open on file descriptor 9.\n", NIL);
  }

  if (cmd != NULL || (shell = getenv("SHELL")) == NULL) shell = "/bin/sh";
  if ((sh0 = strrchr(shell, '/')) == NULL) sh0 = shell; else sh0++;

  /* Start a shell */
  pid = fork();
  if (pid < 0) {
	tell(2, "term: fork() failed: ", strerror(errno), "\n", NIL);
	return;
  }
  if (pid == 0) {
	/* Child: Exec the shell. */
	setgid(getgid());
	setuid(getuid());

	if (commfd != 9) { dup2(commfd, 9); close(commfd); }

	if (cmd == NULL) {
		execl(shell, sh0, (char *) NULL);
	} else {
		execl(shell, sh0, "-c", cmd, (char *) NULL);
	}
	tell(2, "term: can't execute ", shell, ": ", strerror(errno), "\n",NIL);
	_exit(1);
  }
  /* Wait for the shell to exit. */
  isav = signal(SIGINT, SIG_IGN);
  qsav = signal(SIGQUIT, SIG_IGN);
  tsav = signal(SIGTERM, SIG_IGN);
  (void) waitpid(pid, (int *) 0, 0);
  (void) signal(SIGINT, isav);
  (void) signal(SIGQUIT, qsav);
  (void) signal(SIGTERM, tsav);
  tell(1, "\n[back to term]\n", NIL);
}


void lock_device(device)
char *device;
{
/* Lock a device by creating a lock file using SYSV style locking. */

  struct stat stbuf;
  unsigned int pid;
  int fd;
  int n;
  int u;

  if (stat(device, &stbuf) < 0) fatal(device);

  if (!S_ISCHR(stbuf.st_mode)) {
	tell(2, "term: ", device, " is not a character device\n", NIL);
	exit(1);
  }

  /* Compute the lock file name. */
  setnum(lockfile + 23, (stbuf.st_dev >> 8) & 0xFF);	/* FS major (why?) */
  setnum(lockfile + 27, (stbuf.st_rdev >> 8) & 0xFF);	/* device major */
  setnum(lockfile + 31, (stbuf.st_rdev >> 0) & 0xFF);	/* device minor */

  /* Try to make a lock file and put my pid in it. */
  u = umask(0);
  for (;;) {
	if ((fd = open(lockfile, O_RDONLY)) < 0) {
		/* No lock file, try to lock it myself. */
		if (errno != ENOENT) fatal(device);
		if ((fd = open(lockfile, O_WRONLY|O_CREAT|O_EXCL, 0444)) < 0) {
			if (errno == EEXIST) continue;
			fatal(lockfile);
		}
		pid = getpid();
		n = write(fd, &pid, sizeof(pid));
		if (n < 0) {
			n = errno;
			(void) unlink(lockfile);
			errno = n;
			fatal(lockfile);
		}
		close(fd);
		break;
	} else {
		/* Already there, but who owns it? */
		n = read(fd, &pid, sizeof(pid));
		if (n < 0) fatal(device);
		close(fd);
		if (n == sizeof(pid) && !(kill(pid, 0) < 0 && errno == ESRCH)) {
			/* It is locked by a running process. */
			tell(2, "term: ", device,
				" is in use by another program\n", NIL);
			if (getpgrp() == getpid()) sleep(3);
			exit(1);
		}
		/* Stale lock. */
		tell(1, "Removing stale lock ", lockfile, "\n", NIL);
		if (unlink(lockfile) < 0 && errno != ENOENT) fatal(lockfile);
	}
  }
  /* Lock achieved, but what if two terms encounters a stale lock at the same
   * time?
   */
  umask(u);
}


void fatal(char *label)
{
  tell(2, "term: ", label, ": ", strerror(errno), "\n", NIL);
  exit(1);
}


void setnum(char *s, int n)
{
/* Poke 'n' into string 's' backwards as three decimal digits. */
  int i;

  for (i = 0; i < 3; i++) { *--s = '0' + (n % 10); n /= 10; }
}


void set_uart(argc, argv, tcp)
int argc;
char *argv[];
struct termios *tcp;
{
/* Set up the UART parameters. */

  int i;
  char *arg;
  struct param_s *param;

  /* Examine all the parameters and check for validity. */
  for (i = 1; i < argc; ++i) {
	arg = argv[i];
	if (arg[0] == '/' || arg[0] == '-') continue;

	/* Check parameter for legality. */
	for (param = &params[0];
	     param->type != BAD && strcmp(arg, param->pattern) != 0;
	     ++param);
	switch (param->type) {
	    case BAD:
		tell(2, "Invalid parameter: ", arg, "\n", NIL);
		quit(1);
		break;
	    case BITS:
		tcp->c_cflag &= ~CSIZE;
		tcp->c_cflag |= param->value;
		break;
	    case PARITY:
		tcp->c_cflag &= PARENB | PARODD;
		tcp->c_cflag |= param->value;
		break;
	    case SPEED:
		cfsetispeed(tcp, (speed_t) param->value);
		cfsetospeed(tcp, (speed_t) param->value);
		break;
	}
  }
}


void set_raw(tcp)
struct termios *tcp;
{
  /* Set termios attributes for RAW mode. */

  tcp->c_iflag &= ~(ICRNL|IGNCR|INLCR|IXON|IXOFF);
  tcp->c_lflag &= ~(ICANON|IEXTEN|ISIG|ECHO|ECHONL);
  tcp->c_oflag &= ~(OPOST);
  tcp->c_cc[VMIN] = 1;
  tcp->c_cc[VTIME] = 0;
}


void quit(code)
int code;
{
/* Stop the reader process, reset the terminal, and exit. */
  reader(0);
  tcsetattr(0, TCSANOW, &tcsavestdin);
  (void) unlink(lockfile);
  exit(code);
}
