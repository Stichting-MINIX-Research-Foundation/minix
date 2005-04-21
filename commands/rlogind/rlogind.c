/*
rlogind.c

Created:	by Philip Homburg <philip@cs.vu.nl>
Log:		Utmp improvement by Kees Bot <kjb@cs.vu.nl>
		Split to compile easier on i86 by kjb
*/

#include <sys/types.h>
#include <sys/stat.h>
#include <sys/ioctl.h>
#include <errno.h>
#include <fcntl.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <termios.h>
#include <time.h>
#include <unistd.h>
#include <utmp.h>
#include <net/hton.h>
#define EXTERN
#include "rlogind.h"

char pty_str[]= "/dev/ptyXX";
char tty_str[]= "/dev/ttyXX";
char hex_str[16]= "0123456789abcdef";

char PATH_UTMP[] = "/etc/utmp";		/* current logins */
char PATH_WTMP[] = "/usr/adm/wtmp";	/* login/logout history */

char term[64]= "TERM=";
#define ENVSIZE (sizeof("TERM=")-1)	/* skip null for concatenation. */
int confirmed= 0;
char *env[2];
char *args[10];

static void do_child(int tty_fd, char *tty_str);
static void dealloc_term(int slot, char *tty_str, int pid);
static void wtmp(char *user, char *id, char *line, int pid, int type, int slot);
static void setup_term(int fd);
static speed_t num2speed(int num);
static int do_control(char *buf, int cnt);
static void readall(char *buf, int cnt);

int main(int argc, char *argv[])
{
	int error;
	int i, j= 0;
	int tty_fd, pty_fd;
	int login_pid, write_pid;
	int count, bytes, tmp_count;
	char *lp= 0, *cp;
	struct stat struct_stat;
	int slot;

	prog_name= argv[0];

	/* Check if the remote user is allowed in. */
	authenticate();

	write(1, "", 1);	/* Send the '\0' */
	confirmed= 1;

	/* We try to convince the other side not the do ^S/^Q, the rlogin
	 * protocol indicates the we only send this when XOFF is turned off
	 * but we don't know when this happens so we tell the other side that
	 * it is turned off.
	 */
	tcp_urg(1, 1);

	write(1, "\220", 1);

	tcp_urg(1, 0);

	/* Let's look for a pty. */
	pty_fd= -1;
	for (i= 'p'; i <= 'z'; i++)
	{
		pty_str[sizeof(pty_str)-3]= i;
		pty_str[sizeof(pty_str)-2]= '0';
		error= stat(pty_str, &struct_stat);
		if (error == -1)
			continue;
		for (j= 0; j < 16; j++)
		{
			pty_str[sizeof(pty_str)-2]= hex_str[j];
			pty_fd= open(pty_str, O_RDWR);
			if (pty_fd != -1)
				break;
		}
		if (pty_fd != -1)
			break;
	}
	if (pty_fd == -1)
	{
		printf("%s: out of ptys\r\n", prog_name);
		exit(1);
	}
	tty_str[sizeof(pty_str)-3]= i;
	tty_str[sizeof(pty_str)-2]= hex_str[j];

	tty_fd= open(tty_str, O_RDWR);
	if (tty_fd == -1)
	{
		printf("%s: unable to open '%s': %s\r\n", prog_name, tty_str,
			strerror(errno));
		exit(1);
	}

	slot= fttyslot(tty_fd);

	login_pid= fork();
	if (login_pid == -1)
	{
		printf("%s: unable to fork: %s\r\n", prog_name,
			strerror(errno));
		exit(1);
	}
	if (login_pid == 0)
	{
		close(pty_fd);
		wtmp("", "", tty_str, login_pid, LOGIN_PROCESS, slot);
		do_child(tty_fd, tty_str);
	}
	close(tty_fd);

	write_pid= fork();
	if (write_pid == -1)
	{
		printf("%s: unable to fork: %s\r\n", prog_name,
			strerror(errno));
		exit(1);
	}
	if (write_pid == 0)
	{
		dup2(pty_fd, 0);
		count= 0;
		for (;;)
		{
			if (!count)
			{
				count= read(0, line, sizeof(line));
				if (count <= 0)
					break;
				lp= line;
			}
			bytes= write(1, lp, count);
			if (bytes <= 0 || bytes > count)
				break;
			lp += bytes;
			count -= bytes;
		}
		kill(getppid(), SIGKILL);
		dealloc_term(slot, tty_str, login_pid);
		_exit(1);
	}

	dup2(pty_fd, 1);
	count= 0;
	for (;;)
	{
		if (!count)
		{
			count= read(0, line, sizeof(line));
			if (count <= 0)
				break;
			lp= line;
		}
		tmp_count= count;
		cp= memchr(lp, 255, count);
		if (cp)
		{
			tmp_count= cp-lp;
			if (tmp_count == 0)
			{
				tmp_count= do_control(lp, count);
				if (tmp_count)
				{
					lp += tmp_count;
					count -= tmp_count;
					continue;
				}
			}
		}
		bytes= write(1, lp, tmp_count);
		if (bytes <= 0 || bytes > count)
			break;
		lp += bytes;
		count -= bytes;
	}
	kill(write_pid, SIGKILL);
	dealloc_term(slot, tty_str, login_pid);
	return(0);
}

static void do_child(int tty_fd, char *tty_str)
{
	int ctty_fd, tst_fd;
	FILE *tty_file;
	int sav_errno;
	char **argp;

	/* Set up the terminal attributes. */
	setup_term(tty_fd);

	/* Let's start the new session. */
	setsid();
	ctty_fd= open(tty_str, O_RDWR);
	if (ctty_fd == -1)
	{
		printf("%s(do_child): unable to open '%s': %s\r\n",
			prog_name, tty_str, strerror(errno));
		exit(1);
	}
	/* Test if we really got a controlling tty. */
	tst_fd= open("/dev/tty", O_RDWR);
	if (tst_fd == -1)
	{
		printf(
	"%s(do_child): '%s' didn't result in a controlling tty (%s)\r\n",
			prog_name, tty_str, strerror(errno));
		exit(1);
	}

	argp= args;
	*argp++= "login";
	*argp++= "-p";
	*argp++= "-h";
	*argp++= hostname;
	if (authenticated)
		*argp++= "-f";
	if (lusername[0] != '\0')
		*argp++= lusername;

	/* We reached the point of no return. */
	close(tst_fd);
	close(tty_fd);

	if (ctty_fd != 0)
	{
		dup2(ctty_fd, 0);
		close(ctty_fd);
		ctty_fd= 0;
	}
	dup2(ctty_fd, 1);
#if DEBUG
	fprintf(stderr, "execing login\r\n"); 
#endif
	dup2(ctty_fd, 2);
	execve("/bin/login", args, env);
	if (errno == ENOENT) execve("/usr/bin/login", args, env);
	sav_errno= errno;
	tty_file= fdopen(2, "w");
	if (tty_file)
	{
		fprintf(tty_file, "%s(do_child): unable to exec login: %s\r\n",
			prog_name, strerror(sav_errno));
		fflush(tty_file);
	}
	_exit(1);
}

static void dealloc_term(int slot, char *tty_str, int pid)
{
  wtmp("", "", tty_str, pid, DEAD_PROCESS, slot);

  /* Finally we reset the owner and mode of the terminal. */
  chown(tty_str, 0, 0);
  chmod(tty_str, 0666);
}

static void wtmp(
	char *user,		/* name of user */
	char *id,		/* inittab ID */
	char *line,		/* TTY name */
	int pid,		/* PID of process */
	int type,		/* TYPE of entry */
	int slot)		/* slot number in UTMP */
{
/* Log an event into the UTMP and WTMP files. */

  struct utmp utmp;		/* UTMP/WTMP User Accounting */
  int fd= -1;
  int log = 1;			/* log in wtmp */
  char *p;

  /* Strip the /dev part of the TTY name. */
  p = strrchr(line, '/');
  if (p != 0)
	line= p+1;

  if (type == DEAD_PROCESS) {
	/* Don't add a logout entry for just a dying login. */
	if ((fd = open(PATH_UTMP, O_RDONLY)) < 0) return;
	if (lseek(fd, (off_t) slot * sizeof(utmp), SEEK_SET) != -1
		&& read(fd, (void *) &utmp, sizeof(utmp)) == sizeof(utmp))
	{
		if (utmp.ut_type != INIT_PROCESS
				&& utmp.ut_type != USER_PROCESS)
			log= 0;
	}
	close(fd);
  }
  if (type == LOGIN_PROCESS) log= 0;	/* and don't log this one */

  /* Clear the utmp record. */
  memset((void *) &utmp, 0, sizeof(utmp));

  /* Enter new values. */
  strncpy(utmp.ut_name, user, sizeof(utmp.ut_name));
  strncpy(utmp.ut_id, id, sizeof(utmp.ut_id));
  strncpy(utmp.ut_line, line, sizeof(utmp.ut_line));
  strncpy(utmp.ut_host, hostname, sizeof(utmp.ut_host));
  utmp.ut_pid = pid;
  utmp.ut_type = type;
  utmp.ut_time = time((time_t *)0);

  if (log) {
	if ((fd = open(PATH_WTMP, O_WRONLY | O_APPEND)) < 0) return;
	write(fd, (char *) &utmp, sizeof(struct utmp));
	close(fd);
  }

  /* write entry to utmp */
  if ((fd = open(PATH_UTMP, O_WRONLY)) < 0) return;
  if (lseek(fd, (off_t) slot * sizeof(utmp), SEEK_SET) != -1)
	write(fd, (char *) &utmp, sizeof(struct utmp));
  close(fd);
}

void fatal(int fd, char *msg, int err)
{
	int len;
	char buf[80], *bp;

	bp= buf;
	if (!confirmed)
		*bp++= '\1';
	if (err)
		len= sprintf(bp, "rlogind: %s: %s.\r\n", msg, strerror(err));
	else
		len= sprintf(bp, "rlogind: %s.\r\n", msg);
	write(fd, buf, bp+len-buf);
	exit(1);
}

static void setup_term(int fd)
{
	char *cp, *speed;
	struct termios tt;
	speed_t spd;
	int num;
	char *check;

	cp= strchr(term, '/');
	if (cp)
	{
		tcgetattr(fd, &tt);
		*cp++= '\0';
		speed= cp;
		cp= strchr(speed, '/');
		if (cp)
			*cp++= '\0';
		num= strtol(speed, &check, 0);
		spd= num2speed(num);
		if (spd != B0 && check[0] == '\0')
		{
			cfsetospeed(&tt, spd);
			cfsetispeed(&tt, spd);
		}
		tcsetattr(fd, TCSANOW, &tt);
	}
	env[0]= term;
	env[1]= 0;
}

static speed_t num2speed(int num)
{
	static struct 
	{
		int num;
		speed_t value;
	} speed_table[]=
	{
		{ 0, B0, }, { 50, B50, }, { 75, B75, }, { 110, B110, },
		{ 134, B134, }, { 150, B150, }, { 200, B200, }, { 300, B300, },
		{ 600, B600, }, { 1200, B1200, }, { 1800, B1800, },
		{ 2400, B2400, }, { 4800, B4800, }, { 9600, B9600, },
		{ 19200, B19200, }, { 38400, B38400, },
		{ -1, -1 },
	};
	int i;

	for (i= 0; speed_table[i].num != -1; i++)
	{
		if (speed_table[i].num == num)
			return (speed_table[i].value);
	}
	return B0;
}

static int do_control(char *cp, int cnt)
{
	char buf[20];
	struct winsize winsize;

	if (cnt > sizeof(buf))
		cnt= sizeof(buf);

	memcpy(buf, cp, cnt);

	/* Let's fetch the first 2 bytes. */
	if (cnt < 2)
		readall(buf+cnt, 2-cnt);
	if ((unsigned char)buf[1] != 255)
		return 0;

	/* Let's fetch the first 4 bytes. */
	if (cnt < 4)
		readall(buf+cnt, 4-cnt);
	if (buf[2] != 's' || buf[3] != 's')
		return 0;

	/* Let's fetch a winsize structure. */
	if (cnt < 4 + sizeof(winsize))
		readall(buf+cnt, 4 + sizeof(winsize) - cnt);

	memcpy(&winsize, buf+4, sizeof(winsize));
	winsize.ws_row= ntohs(winsize.ws_row);
	winsize.ws_col= ntohs(winsize.ws_col);
	winsize.ws_xpixel= ntohs(winsize.ws_xpixel);
	winsize.ws_ypixel= ntohs(winsize.ws_ypixel);
#if DEBUG
	fprintf(stderr, "setting window size to %d, %d\r\n", winsize.ws_row,
		winsize.ws_col);
#endif
	ioctl(1, TIOCSWINSZ, &winsize);
	return 4 + sizeof(winsize);
}

static void readall(char *buf, int cnt)
{
	int res;

	while(cnt)
	{
		res= read(0, buf, cnt);
		if (res <= 0)
			return;
		buf += cnt;
		cnt -= res;
	}
}
