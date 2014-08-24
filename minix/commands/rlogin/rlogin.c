/*
 * Copyright (c) 1983, 1990 The Regents of the University of California.
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 3. All advertising materials mentioning features or use of this software
 *    must display the following acknowledgement:
 *	This product includes software developed by the University of
 *	California, Berkeley and its contributors.
 * 4. Neither the name of the University nor the names of its contributors
 *    may be used to endorse or promote products derived from this software
 *    without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE REGENTS AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE REGENTS OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 */

#ifndef lint
char copyright[] =
"@(#) Copyright (c) 1983, 1990 The Regents of the University of California.\n\
 All rights reserved.\n";
#endif /* not lint */

#ifndef lint
#ifdef ID
static char sccsid[] = "@(#)rlogin.c	5.33 (Berkeley) 3/1/91";
#endif
#endif /* not lint */

/*
 * $Source$
 * $Header: mit/rlogin/RCS/rlogin.c,v 5.2 89/07/26 12:11:21 kfall
 *	Exp Locker: kfall $
 */

/*
 * rlogin - remote login
 */
#include <sys/ioctl.h>
#include <sys/types.h>
#include <sys/wait.h>

#include <net/netlib.h>
#include <net/hton.h>
#include <net/gen/in.h>
#include <netdb.h>
#include <net/gen/tcp.h>
#include <net/gen/tcp_io.h>

#include <termios.h>
#include <setjmp.h>
#include <errno.h>
#include <pwd.h>
#include <stdio.h>
#include <unistd.h>
#include <string.h>
#include <assert.h>
#include <ctype.h>
#include <fcntl.h>
#include <signal.h>
#include <stdlib.h>

#ifdef KERBEROS
#include <kerberosIV/des.h>
#include <kerberosIV/krb.h>

CREDENTIALS cred;
Key_schedule schedule;
int use_kerberos = 1, doencrypt;
char dst_realm_buf[REALM_SZ], *dest_realm = NULL;
extern char *krb_realmofhost();
#endif

#ifndef TIOCPKT_WINDOW
#define	TIOCPKT_WINDOW	0x80
#endif

/* concession to Sun */
#ifndef SIGUSR1
#define	SIGUSR1	30
#endif

extern int errno;
int eight, litout, rem;

int noescape;
u_char escapechar = '~';

struct speed
{
	speed_t speed;
	char *name;
} speeds[] = {
	{ B0, "0" }, { B50, "50" }, { B75, "75" }, { B110, "110" }, 
	{ B134, "134" }, { B150, "150" }, { B200, "200" }, { B300, "300" }, 
	{ B600, "600" }, { B1200, "1200" }, { B1800, "1800" }, 
	{ B2400, "2400" }, { B4800, "4800" }, { B9600, "9600" }, 
	{ B19200, "19200" }, { B38400, "38400" }, { B57600, "57600" },
	{ B115200, "115200" },
	{ -1, NULL },
};

#if __minix_vmd
/* flow control variables */
int more2read_0;
int inprogress_0;
int more2write_1;
int inprogress_1;
int more2read_rem;
int inprogress_rd_rem;
int more2write_rem;
int inprogress_wr_rem;

/* write to remote */
size_t wr_rem_size;
size_t wr_rem_offset;
size_t extra_wr_rem_size;
size_t extra_wr_rem_offset;
char *extra_wr_rem;
size_t extra_wr_rem_new_size;
char *extra_wr_rem_new;

#endif /* __minix_vmd */

struct	winsize winsize;

#define	get_window_size(fd, wp)	ioctl(fd, TIOCGWINSZ, wp)

extern int main( int argc, char **argv );
static void usage( void ) __attribute__((noreturn));
static u_char getescape( char *p );
static char *speeds2str( speed_t speed );
static void lostpeer( int sig );
static void doit( void );
static void setsignal( int sig, void (*act)(int sig) );
static void msg( char *str );
static void done( int status );
#if !__minix_vmd
static int reader( void );
#endif
static void mode( int f );
#if __minix_vmd
static void mark_async( int fd );
static void init_0( void );
static void init_1( void );
static void init_rd_rem( void );
static void init_wr_rem( void );
static void restart_0( void );
static void restart_1( void );
static void restart_rd_rem( void );
static void restart_wr_rem( void );
static void completed_0( int result, int error );
static void completed_1( int result, int error );
static void completed_rd_rem( int result, int error );
static void completed_wr_rem( int result, int error );
static void do_urg( int urg_byte );
#endif
#if !__minix_vmd
static void catch_child( int sig );
static void writer( void );
#endif
static void echo( int c );
#if __minix_vmd
static void finish( void );
static void sendwindow( void );
static void sigwinch( int sig );
static void subshell( void );
#endif

int main(argc, argv)
	int argc;
	char **argv;
{
	extern char *optarg;
	extern int optind;
	struct passwd *pw;
	struct servent *sp;
	struct termios ttyb;
	nwio_tcpopt_t tcpopt;
	int error;
	int argoff, ch, dflag, one, uid;
	char *host, *p, *user, term[1024];

	argoff = dflag = 0;
	one = 1;
	host = user = NULL;

	if ((p = rindex(argv[0], '/')))
		++p;
	else
		p = argv[0];

	if (strcmp(p, "rlogin"))
		host = p;

	/* handle "rlogin host flags" */
	if (!host && argc > 2 && argv[1][0] != '-') {
		host = argv[1];
		argoff = 1;
	}

#ifdef KERBEROS
#define	OPTIONS	"8EKLde:k:l:x"
#else
#define	OPTIONS	"8EKLde:l:"
#endif
	while ((ch = getopt(argc - argoff, argv + argoff, OPTIONS)) != EOF)
		switch(ch) {
		case '8':
			eight = 1;
			break;
		case 'E':
			noescape = 1;
			break;
		case 'K':
#ifdef KERBEROS
			use_kerberos = 0;
#endif
			break;
		case 'L':
			litout = 1;
			break;
		case 'd':
			dflag = 1;
			break;
		case 'e':
			escapechar = getescape(optarg);
			break;
#ifdef KERBEROS
		case 'k':
			dest_realm = dst_realm_buf;
			(void)strncpy(dest_realm, optarg, REALM_SZ);
			break;
#endif
		case 'l':
			user = optarg;
			break;
#ifdef CRYPT
#ifdef KERBEROS
		case 'x':
			doencrypt = 1;
			des_set_key(cred.session, schedule);
			break;
#endif
#endif
		case '?':
		default:
			usage();
		}
	optind += argoff;
	argc -= optind;
	argv += optind;

	/* if haven't gotten a host yet, do so */
	if (!host && !(host = *argv++))
		usage();

	if (*argv)
		usage();

	if (!(pw = getpwuid(uid = getuid()))) {
		(void)fprintf(stderr, "rlogin: unknown user id.\n");
		exit(1);
	}
	if (!user)
		user = pw->pw_name;

	sp = NULL;
#ifdef KERBEROS
	if (use_kerberos) {
		sp = getservbyname((doencrypt ? "eklogin" : "klogin"), "tcp");
		if (sp == NULL) {
			use_kerberos = 0;
			warning("can't get entry for %s/tcp service",
			    doencrypt ? "eklogin" : "klogin");
		}
	}
#endif
	if (sp == NULL)
		sp = getservbyname("login", "tcp");
	if (sp == NULL) {
		(void)fprintf(stderr, "rlogin: login/tcp: unknown service.\n");
		exit(1);
	}

	(void)strncpy(term, (p = getenv("TERM")) ? p : "network", sizeof(term));
	term[sizeof(term)-1]= 0;

	if (tcgetattr(0, &ttyb) == 0) {
		(void)strcat(term, "/");
		(void)strcat(term, speeds2str(cfgetospeed(&ttyb)));
	}

	(void)get_window_size(0, &winsize);

	(void)signal(SIGPIPE, lostpeer);

#ifdef KERBEROS
try_connect:
	if (use_kerberos) {
		rem = KSUCCESS;
		errno = 0;
		if (dest_realm == NULL)
			dest_realm = krb_realmofhost(host);

#ifdef CRYPT
		if (doencrypt)
			rem = krcmd_mutual(&host, sp->s_port, user, term, 0,
			    dest_realm, &cred, schedule);
		else
#endif /* CRYPT */
			rem = krcmd(&host, sp->s_port, user, term, 0,
			    dest_realm);
		if (rem < 0) {
			use_kerberos = 0;
			sp = getservbyname("login", "tcp");
			if (sp == NULL) {
				(void)fprintf(stderr,
				    "rlogin: unknown service login/tcp.\n");
				exit(1);
			}
			if (errno == ECONNREFUSED)
				warning("remote host doesn't support Kerberos");
			if (errno == ENOENT)
				warning("can't provide Kerberos auth data");
			goto try_connect;
		}
	} else {
#ifdef CRYPT
		if (doencrypt) {
			(void)fprintf(stderr,
			    "rlogin: the -x flag requires Kerberos authentication.\n");
			exit(1);
		}
#endif /* CRYPT */
		rem = rcmd(&host, sp->s_port, pw->pw_name, user, term, 0);
	}
#else
	rem = rcmd(&host, sp->s_port, pw->pw_name, user, term, 0);
#endif /* KERBEROS */

	if (rem < 0)
		exit(1);

	/* Enable BSD compatibility for urgent data. */
	tcpopt.nwto_flags= NWTO_BSD_URG;
	error= ioctl(rem, NWIOSTCPOPT, &tcpopt);
	if (error == -1)
	{
		fprintf(stderr, "rlogin: NWIOSTCPOPT failed: %s\n",
			strerror(errno));
	}

	(void)setuid(uid);
	doit();
	/*NOTREACHED*/
}

struct termios defattr, rawattr;
#if __minix_vmd
int mustsendwindow;
#else
int child;
#endif

static void
doit()
{
	struct termios sb;
#if !__minix_vmd
	int r;
#else
	asio_fd_set_t fd_set;
	struct fwait fw;
	int result;
#endif

	(void)tcgetattr(0, &sb);
	defattr = sb;
	rawattr = sb;

	rawattr.c_iflag &= ~(ICRNL | IGNCR | INLCR | ISTRIP | IXOFF | IXON | 
							PARMRK | IXANY);
	rawattr.c_oflag &= ~(OPOST);
	rawattr.c_lflag &= ~(ECHONL | ECHO | ICANON | IEXTEN | ISIG);

	(void)signal(SIGINT, SIG_IGN);
	setsignal(SIGHUP, exit);
	setsignal(SIGQUIT, exit);

#if !__minix_vmd
	child = fork();
	if (child == -1) {
		(void)fprintf(stderr, "rlogin: fork: %s.\n", strerror(errno));
		done(1);
	}
	if (child == 0) {
		mode(1);
		r = reader();
		if (r == 0) {
			msg("connection closed.");
			exit(0);
		}
		sleep(1);
		msg("\007connection closed.");
		exit(1);
	}

	(void)signal(SIGCHLD, catch_child);
	writer();

#else /* __minix_vmd */

	mode(1);
	/* mark the file descriptors 0, 1, and rem as asynchronous. */
	mark_async(0);
	mark_async(1);
	mark_async(rem);
	init_0();
	init_1();
	init_rd_rem();
	init_wr_rem();

	for (;;)
	{
		ASIO_FD_ZERO(&fd_set);
		fw.fw_flags= 0;
		fw.fw_bits= fd_set.afds_bits;
		fw.fw_maxfd= ASIO_FD_SETSIZE;

		if (more2read_0 && !inprogress_0)
		{
			restart_0();
			fw.fw_flags |= FWF_NONBLOCK;
		}

		if (more2write_1 && !inprogress_1)
		{
			restart_1();
			fw.fw_flags |= FWF_NONBLOCK;
		}

		if (more2read_rem && !inprogress_rd_rem)
		{
			restart_rd_rem();
			fw.fw_flags |= FWF_NONBLOCK;
		}

		if (more2write_rem && !inprogress_wr_rem)
		{
			restart_wr_rem();
			fw.fw_flags |= FWF_NONBLOCK;
		}

		if (more2read_0 && inprogress_0)
			ASIO_FD_SET(0, ASIO_READ, &fd_set);
		if (more2write_1 && inprogress_1)
			ASIO_FD_SET(1, ASIO_WRITE, &fd_set);
		if (more2read_rem && inprogress_rd_rem)
			ASIO_FD_SET(rem, ASIO_READ, &fd_set);
		if (more2write_rem && inprogress_wr_rem)
			ASIO_FD_SET(rem, ASIO_WRITE, &fd_set);

		for (;;)
		{
			result= fwait(&fw);
			if (result == -1 && (errno == EAGAIN || 
							errno == EINTR))
			{
				break;
			}
			if (result == -1)
			{
				fprintf(stderr, "fwait failed (%s)\n", 
							strerror(errno));
				exit(1);
			}
			assert(result == 0);
#if 0
printf("fwait: fw_fw= %d, fw_operation= %d, fw_result= %d, fw.fw_errno= %d\n",
	fw.fw_fd, fw.fw_operation, fw.fw_result, fw.fw_errno);
#endif
			if (fw.fw_fd == 0 && fw.fw_operation == ASIO_READ)
			{
				completed_0(fw.fw_result, fw.fw_errno);
			}
			else if (fw.fw_fd == 1 && 
					fw.fw_operation == ASIO_WRITE)
			{
				completed_1(fw.fw_result, fw.fw_errno);
			}
			else if (fw.fw_fd == rem && 
					fw.fw_operation == ASIO_READ)
			{
				completed_rd_rem(fw.fw_result, fw.fw_errno);
			}
			else if (fw.fw_fd == rem && 
					fw.fw_operation == ASIO_WRITE)
			{
				completed_wr_rem(fw.fw_result, fw.fw_errno);
			}
			else
			{
				fprintf(stderr,
			"strange result from fwait: fd= %d, operation= %d\n",
					fw.fw_fd, fw.fw_operation);
				exit(1);
			}
			if (!(fw.fw_flags & FWF_MORE))
				break;
		}
		if (mustsendwindow)
		{
			mustsendwindow= 0;
			sendwindow();
		}
	}
#endif /* __minix_vmd */
	msg("connection closed.");
	done(0);
}

/* trap a signal, unless it is being ignored. */
static void
setsignal(sig, act)
	int sig;
 void(*act) ( int sig );
{
	if (signal(sig, act) == SIG_IGN)
		(void)signal(sig, SIG_IGN);
}

static void
done(status)
	int status;
{
	int w, wstatus;

	mode(0);
#if !__minix_vmd
	if (child > 0) {
		/* make sure catch_child does not snap it up */
		(void)signal(SIGCHLD, SIG_DFL);
		if (kill(child, SIGKILL) >= 0)
			while ((w = wait(&wstatus)) > 0 && w != child);
	}
#endif
	exit(status);
}

int dosigwinch;
#if !__minix
void sigwinch();
#endif

#if !__minix_vmd
static void
catch_child(sig)
	int sig;
{
	int status;
	int pid;

	for (;;) {
		pid = waitpid(-1, &status, WNOHANG|WUNTRACED);
		if (pid == 0)
			return;
		/* if the child (reader) dies, just quit */
		if (pid < 0 || ((pid == child) && (!WIFSTOPPED(status))))
			done(WTERMSIG(status) | WEXITSTATUS(status));
	}
	/* NOTREACHED */
}
#endif

#if !__minix_vmd
/*
 * writer: write to remote: 0 -> line.
 * ~.				terminate
 * ~^Z				suspend rlogin process.
 * ~<delayed-suspend char>	suspend rlogin process, but leave reader alone.
 */
static void
writer()
{
	register int bol, local, n;
	u_char ch;
	int c;

	bol = 1;			/* beginning of line */
	local = 0;
	for (;;) {
		n = read(STDIN_FILENO, &ch, 1);
		if (n <= 0) {
			if (n < 0 && errno == EINTR)
				continue;
			break;
		}
		c = ch;
		/*
		 * If we're at the beginning of the line and recognize a
		 * command character, then we echo locally.  Otherwise,
		 * characters are echo'd remotely.  If the command character
		 * is doubled, this acts as a force and local echo is
		 * suppressed.
		 */
		if (bol) {
			bol = 0;
			if (!noescape && c == escapechar) {
				local = 1;
				continue;
			}
		} else if (local) {
			local = 0;
			if (c == '.' || c == defattr.c_cc[VEOF]) {
				echo(c);
				break;
			}
#if !__minix
			if (c == defattr.c_cc[VSUSP]) {
				bol = 1;
				echo(c);
				stop(c);
				continue;
			}
#endif
			if (c != escapechar)
#ifdef CRYPT
#ifdef KERBEROS
				if (doencrypt)
					(void)des_write(rem, &escapechar, 1);
				else
#endif
#endif
					(void)write(rem, &escapechar, 1);
		}

		ch = c;
#ifdef CRYPT
#ifdef KERBEROS
		if (doencrypt) {
			if (des_write(rem, &ch, 1) == 0) {
				msg("line gone");
				break;
			}
		} else
#endif
#endif
			if (write(rem, &ch, 1) == 0) {
				msg("line gone");
				break;
			}
		bol = c == defattr.c_cc[VKILL] ||
		    c == defattr.c_cc[VEOF] ||
		    c == defattr.c_cc[VINTR] ||
		    c == defattr.c_cc[VSUSP] ||
		    c == '\r' || c == '\n';
	}
}
#endif

#if !__minix_vmd
static void
echo(c)
int c;
{
	register char *p;
	char buf[8];

	p = buf;
	c &= 0177;
	*p++ = escapechar;
	if (c < ' ') {
		*p++ = '^';
		*p++ = c + '@';
	} else if (c == 0177) {
		*p++ = '^';
		*p++ = '?';
	} else
		*p++ = c;
	*p++ = '\r';
	*p++ = '\n';
	(void)write(STDOUT_FILENO, buf, p - buf);
}
#endif

#if !__minix
stop(cmdc)
	char cmdc;
{
	mode(0);
	(void)signal(SIGCHLD, SIG_IGN);
	(void)kill(cmdc == defltc.t_suspc ? 0 : getpid(), SIGTSTP);
	(void)signal(SIGCHLD, catch_child);
	mode(1);
	sigwinch();			/* check for size changes */
}
#endif

#if __minix_vmd
#ifdef SIGWINCH
static void
sigwinch(sig)
	int sig;
{
	struct winsize ws;

#if __minix
	signal(SIGWINCH, sigwinch);
#endif

	if (dosigwinch && get_window_size(0, &ws) == 0 &&
	    memcmp(&ws, &winsize, sizeof(ws))) {
		winsize = ws;
		mustsendwindow= 1;
	}
}

/*
 * Send the window size to the server via the magic escape
 */
static void
sendwindow()
{
	struct winsize *wp;
	char *obuf, *new_buf;

	new_buf= realloc(extra_wr_rem_new, 
					extra_wr_rem_new_size+4+sizeof(*wp));
	if (new_buf == 0)
		return;
	extra_wr_rem_new= new_buf;
	obuf= new_buf+extra_wr_rem_new_size;
	extra_wr_rem_new_size += 4+sizeof(*wp);

	more2read_0= 0;
	more2write_rem= 1;

	wp = (struct winsize *)(obuf+4);
	obuf[0] = 0377;
	obuf[1] = 0377;
	obuf[2] = 's';
	obuf[3] = 's';
	wp->ws_row = htons(winsize.ws_row);
	wp->ws_col = htons(winsize.ws_col);
	wp->ws_xpixel = htons(winsize.ws_xpixel);
	wp->ws_ypixel = htons(winsize.ws_ypixel);
}
#endif /* SIGWINCH */
#endif

#if !__minix_vmd
/*
 * reader: read from remote: line -> 1
 */
#define	READING	1
#define	WRITING	2

int rcvcnt, rcvstate;
char rcvbuf[8 * 1024];

static int
reader()
{
	int n, remaining;
	char *bufp = rcvbuf;

	for (;;) {
		while ((remaining = rcvcnt - (bufp - rcvbuf)) > 0) {
			rcvstate = WRITING;
			n = write(STDOUT_FILENO, bufp, remaining);
			if (n < 0) {
				if (errno != EINTR)
					return(-1);
				continue;
			}
			bufp += n;
		}
		bufp = rcvbuf;
		rcvcnt = 0;
		rcvstate = READING;

#ifdef CRYPT
#ifdef KERBEROS
		if (doencrypt)
			rcvcnt = des_read(rem, rcvbuf, sizeof(rcvbuf));
		else
#endif
#endif
			rcvcnt = read(rem, rcvbuf, sizeof (rcvbuf));
		if (rcvcnt == 0)
			return (0);
		if (rcvcnt < 0) {
			if (errno == EINTR)
				continue;
			if (errno == EURG) {
				nwio_tcpopt_t tcpopt;
#if DEBUG
fprintf(stderr, "\n\rEURG\n\r");
#endif
				tcpopt.nwto_flags= NWTO_RCV_URG;
				if (ioctl(rem, NWIOSTCPOPT, &tcpopt) == -1) {
					fprintf(stderr,
				"rlogin: trouble with urgent data: %s\n",
						strerror(errno));
					return(-1);
				}
				continue;
			}
			if (errno == ENOURG) {
				nwio_tcpopt_t tcpopt;
#if DEBUG
fprintf(stderr, "\n\rENOURG\n\r");
#endif
				tcpopt.nwto_flags= NWTO_RCV_NOTURG;
				if (ioctl(rem, NWIOSTCPOPT, &tcpopt) == -1) {
					fprintf(stderr,
				"rlogin: trouble with not-urgent data: %s\n",
						strerror(errno));
					return(-1);
				}
				continue;
			}
			(void)fprintf(stderr, "rlogin: read: %s\n",
				strerror(errno));
			return(-1);
		}
	}
}
#endif  /* !__minix_vmd */

static void
mode(f)
	int f;
{
	struct termios *sb;

	switch(f) {
	case 0:
		sb= &defattr;
		break;
	case 1:
		sb= &rawattr;
		break;
	default:
		return;
	}
	(void)tcsetattr(0, TCSAFLUSH, sb);
}

static void
lostpeer(sig)
int sig;
{
	(void)signal(SIGPIPE, SIG_IGN);
	msg("\007connection closed.");
	done(1);
}

static void
msg(str)
	char *str;
{
	(void)fprintf(stderr, "rlogin: %s\r\n", str);
}

#ifdef KERBEROS
/* VARARGS */
warning(va_alist)
va_dcl
{
	va_list ap;
	char *fmt;

	(void)fprintf(stderr, "rlogin: warning, using standard rlogin: ");
	va_start(ap);
	fmt = va_arg(ap, char *);
	vfprintf(stderr, fmt, ap);
	va_end(ap);
	(void)fprintf(stderr, ".\n");
}
#endif

static void
usage()
{
	(void)fprintf(stderr,
	    "Usage: rlogin [-%s]%s[-e char] [-l username] host\n",
#ifdef KERBEROS
#ifdef CRYPT
	    "8ELx", " [-k realm] ");
#else
	    "8EL", " [-k realm] ");
#endif
#else
	    "8EL", " ");
#endif
	exit(1);
}

/*
 * The following routine provides compatibility (such as it is) between 4.2BSD
 * Suns and others.  Suns have only a `ttysize', so we convert it to a winsize.
 */
#ifdef sun
get_window_size(fd, wp)
	int fd;
	struct winsize *wp;
{
	struct ttysize ts;
	int error;

	if ((error = ioctl(0, TIOCGSIZE, &ts)) != 0)
		return(error);
	wp->ws_row = ts.ts_lines;
	wp->ws_col = ts.ts_cols;
	wp->ws_xpixel = 0;
	wp->ws_ypixel = 0;
	return(0);
}
#endif

static u_char
getescape(p)
	register char *p;
{
	long val;
	int len;

	if ((len = strlen(p)) == 1)	/* use any single char, including '\' */
		return((u_char)*p);
					/* otherwise, \nnn */
	if (*p == '\\' && len >= 2 && len <= 4) {
		val = strtol(++p, (char **)NULL, 8);
		for (;;) {
			if (!*++p)
				return((u_char)val);
			if (*p < '0' || *p > '8')
				break;
		}
	}
	msg("illegal option value -- e");
	usage();
	/* NOTREACHED */
}

static char *
speeds2str(speed)
	speed_t speed;
{
	int i;
	for (i= 0; speeds[i].name != NULL && speeds[i].speed != speed; i++) {
		if (speeds[i].speed == speed) return speeds[i].name;
	}
	return "unknown";
}

#if __minix_vmd
static void 
mark_async(fd)
	int fd;
{
	int result;
	int v;

	result= fcntl(fd, F_GETFD);
	if (result == -1)
	{
		fprintf(stderr,
			"rlogin: mark_async: fcntl(%d, GETFD) failed (%s)\n",
			fd, strerror(errno));
		exit(1);
	}
	v= result | FD_ASYNCHIO;
	result= fcntl(fd, F_SETFD, v);
	if (result == -1)
	{
		fprintf(stderr, 
		"rlogin: mark_async: fcntl(%d, SETFD, %d) failed (%s)\n",
			fd, v, strerror(errno));
		exit(1);
	}
}

#define RD_0_BUFSIZE	256
char rd_0_buf[RD_0_BUFSIZE];
size_t rd_0_offset;

static void
init_0()
{
	more2read_0= 1;
	inprogress_0= 0;
	rd_0_offset= 0;
}

size_t wr_1_size;
size_t wr_1_offset;
char *urg_1;
size_t urg_1_size;
char *extra_1;
size_t extra_1_size;
size_t extra_1_offset;
char *extra_1_new;
size_t extra_1_new_size;
#define MAX_EXTRA_1_NEW_SIZE	(16*1024)

static void
init_1()
{
	more2write_1= 0;
	inprogress_1= 0;
	wr_1_size= 0;
	wr_1_offset= 0;
	urg_1= NULL;
	urg_1_size= 0;
	extra_1= NULL;
	extra_1_size= 0;
	extra_1_offset= 0;
	extra_1_new= NULL;
	extra_1_new_size= 0;
}

#define RD_REM_BUFSIZE	(8*1024)
char rd_rem_buf[RD_REM_BUFSIZE];
size_t rd_rem_offset;
int rd_rem_urg;

static void 
init_rd_rem()
{
	more2read_rem= 1;
	inprogress_rd_rem= 0;
	rd_rem_offset= 0;
	rd_rem_urg= 0;
}

static void 
init_wr_rem()
{
	more2write_rem= 0;
	inprogress_wr_rem= 0;
	wr_rem_size= 0;
	wr_rem_offset= 0;
	extra_wr_rem_size= 0;
	extra_wr_rem_offset= 0;
	extra_wr_rem= NULL;
	extra_wr_rem_new_size= 0;
	extra_wr_rem_new= NULL;
}

static void
restart_0()
{
	size_t offset;
	int result, error;

	assert(!inprogress_0);
	rd_0_offset= 1;
	offset= 0;
	while (offset < RD_0_BUFSIZE)
	{
		result= read(0, rd_0_buf+rd_0_offset+offset,
					RD_0_BUFSIZE-rd_0_offset-offset);
		if (result > 0)
		{
			offset += result;
			assert(rd_0_offset+offset <= RD_0_BUFSIZE);
			continue;
		}
		error= errno;

		if (offset != 0)
			completed_0(offset, 0);
		rd_0_offset += offset;
		if (result == -1 && error == EINPROGRESS)
		{
			inprogress_0= 1;
			return;
		}
		completed_0(result, error);
		return;
	}
	completed_0(offset, 0);
}

static void 
restart_1()
{
	size_t offset;
	int result, error;

	assert(!inprogress_1);

	while (extra_1 != NULL || extra_1_new != NULL)
	{
		if (extra_1 == NULL)
		{
			extra_1= extra_1_new;
			extra_1_new= NULL;
			extra_1_size= extra_1_new_size;
			extra_1_new_size= 0;
			extra_1_offset= 0;
		}
		offset= 0;
#if DEBUG
		if (extra_1_size == 0)
			fprintf(stderr, "restart_1: extra_1_size= 0\n");
#endif
		while (offset < extra_1_size)
		{
			result= write(1, extra_1+extra_1_offset+offset, 
							extra_1_size-offset);
			if (result > 0)
			{
				assert (result <= extra_1_size-offset);
				offset += result;
				continue;
			}
			error= errno;
			if (offset != 0)
				completed_1(offset, 0);

			if (result == -1 && errno == EINPROGRESS)
			{
				inprogress_1= 1;
				return;
			}
			completed_1(result, errno);
			return;
		}
		completed_1(offset, 0);
	}

	offset= 0;

	if (wr_1_size == 0)
	{
		more2write_1= 0;
		more2read_rem= 1;
		return;
	}

	while (offset < wr_1_size)
	{
		result= write(1, rd_rem_buf+wr_1_offset+offset, 
							wr_1_size-offset);
		if (result > 0)
		{
			assert (result <= wr_1_size-offset);
			offset += result;
			continue;
		}
		error= errno;
		if (offset != 0)
			completed_1(offset, 0);

		if (result == -1 && errno == EINPROGRESS)
		{
			inprogress_1= 1;
			return;
		}
		completed_1(result, errno);
		return;
	}
	completed_1(offset, 0);
}

static void
restart_rd_rem()
{
	size_t offset;
	int result, error;

	assert(!inprogress_rd_rem);
	rd_rem_offset= 0;
	offset= 0;
	while (offset < RD_REM_BUFSIZE)
	{
		result= read(rem, rd_rem_buf+offset, RD_REM_BUFSIZE-offset);
		if (result > 0)
		{
			offset += result;
			assert(offset <= RD_REM_BUFSIZE);
			continue;
		}
		error= errno;

		if (offset != 0)
			completed_rd_rem(offset, 0);
		rd_rem_offset= offset;
		if (result == -1 && error == EINPROGRESS)
		{
			inprogress_rd_rem= 1;
			return;
		}
		completed_rd_rem(result, error);
		return;
	}
	completed_rd_rem(offset, 0);
}

static void
restart_wr_rem()
{
	size_t offset;
	int result, error;

	assert(!inprogress_wr_rem);

	if (extra_wr_rem_new != NULL && extra_wr_rem == NULL)
	{
		extra_wr_rem= extra_wr_rem_new;
		extra_wr_rem_size= extra_wr_rem_new_size;
		extra_wr_rem_offset= 0;
		extra_wr_rem_new= NULL;
		extra_wr_rem_new_size= 0;
	}
	if (extra_wr_rem != NULL)
	{
		offset= 0;
		while (offset < extra_wr_rem_size)
		{
			result= write(rem, 
				extra_wr_rem+extra_wr_rem_offset+offset, 
						extra_wr_rem_size-offset);
			if (result > 0)
			{
				assert (result <= extra_wr_rem_size-offset);
				offset += result;
				continue;
			}
			error= errno;
			if (offset != 0)
				completed_wr_rem(offset, 0);

			if (result == -1 && errno == EINPROGRESS)
			{
				inprogress_wr_rem= 1;
				return;
			}
			completed_wr_rem(result, errno);
			return;
		}
		completed_wr_rem(offset, 0);
	}
	if (wr_rem_size == 0)
		return;

	offset= 0;
	while (offset < wr_rem_size)
	{
		result= write(rem, rd_0_buf+wr_rem_offset+offset, 
							wr_rem_size-offset);
		if (result > 0)
		{
			assert (result <= wr_rem_size-offset);
			offset += result;
			continue;
		}
		error= errno;
		if (offset != 0)
			completed_wr_rem(offset, 0);

		if (result == -1 && errno == EINPROGRESS)
		{
			inprogress_wr_rem= 1;
			return;
		}
		completed_wr_rem(result, errno);
		return;
	}
	completed_wr_rem(offset, 0);
}

static void
completed_0(result, error)
	int result;
	int error;
{
	static int bol= 0, local= 0;

	char *iptr, *optr;
	int i;
	u_char c;

	inprogress_0= 0;

	if (result > 0)
	{
		assert(rd_0_offset > 0);
		wr_rem_offset= 1;

		iptr= rd_0_buf+rd_0_offset;
		optr= rd_0_buf+wr_rem_offset;
		for (i= 0; i<result; iptr++, i++)
		{
			c= *iptr;
			if (bol)
			{
				bol= 0;
				if (!noescape && c == escapechar)
				{
					local= 1;
					continue;
				}
			}
			else if (local)
			{
				local= 0;
				if (c == '.' || (c != _POSIX_VDISABLE &&
					c == defattr.c_cc[VEOF]))
				{
					echo(c);
					finish();
					/* NOTREACHED */
				}
				if (c == '!')
				{
					subshell();
					continue;
				}
				if (c != escapechar)
				{
					if (optr < iptr)
					{
						*(optr++)= escapechar;
					}
					else
					{
						assert(optr == iptr);
						assert(iptr == rd_0_buf+
								rd_0_offset);
						assert(rd_0_offset > 0);
						wr_rem_offset--;
						optr[-1]= escapechar;
					}
				}
			}
			*(optr++)= c;
			bol= (c != _POSIX_VDISABLE) && 
				(c == defattr.c_cc[VKILL] ||
				c == defattr.c_cc[VEOF] ||
				c == defattr.c_cc[VINTR] ||
				c == defattr.c_cc[VSUSP] ||
				c == '\r' || c == '\n');
		}
		wr_rem_size += optr-rd_0_buf-wr_rem_offset;
		if (wr_rem_size != 0)
		{
			more2read_0= 0;
			more2write_rem= 1;
		}
		return;
	} else
	if (result < 0) {
		fprintf(stderr, "rlogin: %s\n", strerror(error));
	}
	done(1);
}

static void
completed_1(result, error)
	int result;
	int error;
{
	inprogress_1= 0;

	if (result > 0)
	{
		if (extra_1 != NULL)
		{
			assert (result <= extra_1_size);
			extra_1_size -= result;
			extra_1_offset += result;
			if (extra_1_size == 0)
			{
				more2write_1= 0;
				more2read_rem= 1;
				free(extra_1);
				extra_1= NULL;
			}
			return;
		}
		assert (result <= wr_1_size);
		wr_1_size -= result;
		wr_1_offset += result;
		if (wr_1_size == 0)
		{
			more2write_1= 0;
			more2read_rem= 1;
		}
		return;
	} else
	if (result < 0) {
		fprintf(stderr, "rlogin: %s\n", strerror(error));
	}
	done(1);
}

static void
completed_rd_rem(result, error)
	int result;
	int error;
{
	nwio_tcpopt_t tcpopt;
	char *new_buf;
	size_t keep_size;
	u_char urg_byte;
	int i;

	inprogress_rd_rem= 0;

	if (result > 0)
	{
		if (rd_rem_urg)
		{
#if DEBUG
fprintf(stderr, "\n\r%d urg bytes\n\r", result);
#endif
			if (urg_1_size > MAX_EXTRA_1_NEW_SIZE)
			{
				keep_size= MAX_EXTRA_1_NEW_SIZE/2;
				memmove(urg_1, urg_1+urg_1_size-keep_size, 
					keep_size);
				urg_1_size= keep_size;
			}
			new_buf= realloc(urg_1, urg_1_size+result);
			if (new_buf == NULL)
			{
				fprintf(stderr, 
					"rlogin: warning realloc %d failed\n",
					urg_1_size+result);
				return;
			}
			memcpy(new_buf+urg_1_size, 
				rd_rem_buf+rd_rem_offset, result);
			urg_1= new_buf;
			urg_1_size += result;
			return;
		}
		more2read_rem= 0;
		more2write_1= 1;
		wr_1_size= result;
		wr_1_offset= rd_rem_offset;
		return;
	}
	if (result == -1 && error == EURG)
	{
#if DEBUG
fprintf(stderr, "\n\rEURG\n\r");
#endif
		rd_rem_urg= 1;
		tcpopt.nwto_flags= NWTO_RCV_URG;
		result= ioctl(rem, NWIOSTCPOPT, &tcpopt);
		if (result == -1)
		{
			fprintf(stderr, 
				"rlogin: NWIOSTCPOPT on %d failed (%s)\n",
				rem, strerror(errno));
			exit(1);
		}
		return;
	}
	if (result == -1 && error == ENOURG)
	{
#if DEBUG
fprintf(stderr, "\n\rENOURG\n\r");
#endif
		rd_rem_urg= 0;
		tcpopt.nwto_flags= NWTO_RCV_NOTURG;
		result= ioctl(rem, NWIOSTCPOPT, &tcpopt);
		if (result == -1)
		{
			fprintf(stderr, 
				"rlogin: NWIOSTCPOPT on %d failed (%s)\n",
				rem, strerror(errno));
			exit(1);
		}
		if (urg_1_size != 0)
		{
			urg_byte= urg_1[urg_1_size-1];
			urg_1_size--;
			do_urg(urg_byte);
			if (urg_1_size == 0)
				return;
			if (extra_1_new_size + urg_1_size > MAX_EXTRA_1_NEW_SIZE)
			{
				extra_1_new_size= 0;
				free(extra_1_new);
				extra_1_new= NULL;
			}
			if (extra_1_new_size != 0)
			{
				new_buf= realloc(extra_1_new, 
					extra_1_new_size+urg_1_size);
				if (new_buf == 0)
				{
					extra_1_new_size= 0;
					free(extra_1_new);
					extra_1_new= NULL;
				}
				else
				{
					extra_1_new= new_buf;
					memcpy(extra_1_new+extra_1_new_size,
						urg_1, urg_1_size);
					extra_1_new_size += urg_1_size;
					urg_1_size= 0;
					free(urg_1);
					urg_1= NULL;
				}
			}
			if (extra_1_new_size == 0)
			{
				extra_1_new_size= urg_1_size;
				extra_1_new= urg_1;
				urg_1_size= 0;
				urg_1= NULL;
			}
			more2read_rem= 0;
			more2write_1= 1;
		}
		return;
	}
	if (result == -1 && error == EINTR)
	{
		/* Never mind. */
		return;
	}
	if (result == 0)
	{
		msg("connection closed.");
		done(0);
	}
	if (result < 0) {
		fprintf(stderr, "rlogin: %s\n", strerror(error));
	}
	done(1);
}

static void
completed_wr_rem(result, error)
	int result;
	int error;
{
	inprogress_wr_rem= 0;

	if (result > 0)
	{
		if (extra_wr_rem != NULL)
		{
			assert (result <= extra_wr_rem_size);
			extra_wr_rem_size -= result;
			extra_wr_rem_offset += result;
			if (extra_wr_rem_size == 0)
			{
				free(extra_wr_rem);
				extra_wr_rem= NULL;
				if (wr_rem_size == 0)
				{
					more2write_rem= 0;
					more2read_0= 1;
				}
			}
			return;
		}

		assert (result <= wr_rem_size);
		wr_rem_size -= result;
		wr_rem_offset += result;
		if (wr_rem_size == 0)
		{
			more2write_rem= 0;
			more2read_0= 1;
		}
		return;
	}
	if (result < 0) {
		fprintf(stderr, "rlogin: %s\n", strerror(error));
	}
	done(1);
}

static void
do_urg(urg_byte)
	int urg_byte;
{
#if DEBUG
	fprintf(stderr, "rlogin: warning got urg_byte 0x%x\r\n", urg_byte);
#endif
	if (urg_byte & TIOCPKT_WINDOW)
	{
		if (dosigwinch == 0)
		{
			sendwindow();
			signal(SIGWINCH, sigwinch);
		}
		dosigwinch= 1;
	}
}

static void
echo(c)
	int c;
{
	u_char c1;
	char *new_buf;

	new_buf= realloc(extra_1_new, extra_1_new_size+6);
	if (new_buf == NULL)
		return;
	extra_1_new= new_buf;
	new_buf= extra_1_new+extra_1_new_size;

	c1= escapechar;
	if (c1 < ' ')
	{
		*new_buf++= '^';
		*new_buf++= c1 + '@';
	}
	else if (c1 == 0x7f)
	{
		*new_buf++= '^';
		*new_buf++= '?';
	}
	else
		*new_buf++= c1;

	if (c < ' ')
	{
		*new_buf++= '^';
		*new_buf++= c + '@';
	}
	else if (c == 0x7f)
	{
		*new_buf++= '^';
		*new_buf++= '?';
	}
	else
		*new_buf++= c;

	*new_buf++= '\r';
	*new_buf++= '\n';
	extra_1_new_size= new_buf-extra_1_new;
	more2write_1= 1;
}

static void
finish()
{
	done(0);
}

static char cmdbuf[256];

static void
subshell()
{
	/* Start a subshell. Based on the first character of the command,
	 * the tcp connection will be present at fd 3 ('+'), or at
	 * fd 0 and fd 1 ('=')
	 */
	int r, pid, stat, len;
	char *shell, *cmd;

	/* cancel the reads and writes that are in progress. */
	if (inprogress_0)
	{
		r= fcancel(0, ASIO_READ);
		if (r != 0) abort();
	}
	if (inprogress_1)
	{
		r= fcancel(1, ASIO_WRITE);
		if (r != 0) abort();
	}
	if (inprogress_rd_rem)
	{
		r= fcancel(rem, ASIO_READ);
		if (r != 0) abort();
	}
	if (inprogress_wr_rem)
	{
		r= fcancel(rem, ASIO_WRITE);
		if (r != 0) abort();
	}

	mode(0);

	pid= fork();
	if (pid == -1) abort();
	if (pid != 0)
	{
		r= waitpid(pid, &stat, 0);
		if (r != pid) abort();

#if DEBUG
		fprintf(stderr, "stat: 0x%x\n", stat);
#endif
		mode(1);
		return;
	}

	(void)signal(SIGINT, SIG_DFL);

	shell= getenv("SHELL");
	if (shell == NULL)
		shell= "/bin/sh";
	printf("~!\ncommand [%s]: ", shell);
	cmd= fgets(cmdbuf, sizeof(cmdbuf), stdin);
	if (cmd == NULL)
		exit(0);
#if DEBUG
	printf("got command '%s'\n", cmd);
#endif

	/* Strip the trailing newline */
	len= strlen(cmd);
	if (len > 0 && cmd[len-1] == '\n')
		cmd[len-1]= '\0';
	else
		printf("\n");

	/* Skip leading white space */
	while (*cmd != '\0' && isspace(*cmd))
		cmd++;

	if (*cmd == '+')
	{
		if (rem != 3)
		{
			dup2(rem, 3);
			close(rem);
		}
		cmd++;
	}
	else if (*cmd == '=')
	{
		dup2(rem, 0);
		dup2(rem, 1);
		close(rem);
		cmd++;
	}
	else
		close(rem);
	if (*cmd == '\0')
	{
		r= execl(shell, shell, NULL);
	}
	else
	{
		r= execl("/bin/sh", "sh", "-c", cmd, NULL);
	}
	printf("exec failed: %d, %d\n", r, errno);
	exit(0);
}
#endif /* __minix_vmd */
