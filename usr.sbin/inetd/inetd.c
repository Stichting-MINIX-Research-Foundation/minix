/*	$NetBSD: inetd.c,v 1.122 2014/04/05 23:36:10 khorben Exp $	*/

/*-
 * Copyright (c) 1998, 2003 The NetBSD Foundation, Inc.
 * All rights reserved.
 *
 * This code is derived from software contributed to The NetBSD Foundation
 * by Jason R. Thorpe of the Numerical Aerospace Simulation Facility,
 * NASA Ames Research Center and by Matthias Scheler.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY THE NETBSD FOUNDATION, INC. AND CONTRIBUTORS
 * ``AS IS'' AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED
 * TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL THE FOUNDATION OR CONTRIBUTORS
 * BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 * CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
 * POSSIBILITY OF SUCH DAMAGE.
 */

/*
 * Copyright (c) 1983, 1991, 1993, 1994
 *	The Regents of the University of California.  All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 3. Neither the name of the University nor the names of its contributors
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

#include <sys/cdefs.h>
#ifndef lint
__COPYRIGHT("@(#) Copyright (c) 1983, 1991, 1993, 1994\
 The Regents of the University of California.  All rights reserved.");
#if 0
static char sccsid[] = "@(#)inetd.c	8.4 (Berkeley) 4/13/94";
#else
__RCSID("$NetBSD: inetd.c,v 1.122 2014/04/05 23:36:10 khorben Exp $");
#endif
#endif /* not lint */

/*
 * Inetd - Internet super-server
 *
 * This program invokes all internet services as needed.  Connection-oriented
 * services are invoked each time a connection is made, by creating a process.
 * This process is passed the connection as file descriptor 0 and is expected
 * to do a getpeername to find out the source host and port.
 *
 * Datagram oriented services are invoked when a datagram
 * arrives; a process is created and passed a pending message
 * on file descriptor 0.  Datagram servers may either connect
 * to their peer, freeing up the original socket for inetd
 * to receive further messages on, or ``take over the socket'',
 * processing all arriving datagrams and, eventually, timing
 * out.	 The first type of server is said to be ``multi-threaded'';
 * the second type of server ``single-threaded''.
 *
 * Inetd uses a configuration file which is read at startup
 * and, possibly, at some later time in response to a hangup signal.
 * The configuration file is ``free format'' with fields given in the
 * order shown below.  Continuation lines for an entry must being with
 * a space or tab.  All fields must be present in each entry.
 *
 *	service name			must be in /etc/services or must
 *					name a tcpmux service
 *	socket type[:accf[,arg]]	stream/dgram/raw/rdm/seqpacket,
					only stream can name an accept filter
 *	protocol			must be in /etc/protocols
 *	wait/nowait[:max]		single-threaded/multi-threaded, max #
 *	user[:group]			user/group to run daemon as
 *	server program			full path name
 *	server program arguments	maximum of MAXARGS (20)
 *
 * For RPC services
 *      service name/version            must be in /etc/rpc
 *	socket type			stream/dgram/raw/rdm/seqpacket
 *	protocol			must be in /etc/protocols
 *	wait/nowait[:max]		single-threaded/multi-threaded
 *	user[:group]			user to run daemon as
 *	server program			full path name
 *	server program arguments	maximum of MAXARGS (20)
 *
 * For non-RPC services, the "service name" can be of the form
 * hostaddress:servicename, in which case the hostaddress is used
 * as the host portion of the address to listen on.  If hostaddress
 * consists of a single `*' character, INADDR_ANY is used.
 *
 * A line can also consist of just
 *	hostaddress:
 * where hostaddress is as in the preceding paragraph.  Such a line must
 * have no further fields; the specified hostaddress is remembered and
 * used for all further lines that have no hostaddress specified,
 * until the next such line (or EOF).  (This is why * is provided to
 * allow explicit specification of INADDR_ANY.)  A line
 *	*:
 * is implicitly in effect at the beginning of the file.
 *
 * The hostaddress specifier may (and often will) contain dots;
 * the service name must not.
 *
 * For RPC services, host-address specifiers are accepted and will
 * work to some extent; however, because of limitations in the
 * portmapper interface, it will not work to try to give more than
 * one line for any given RPC service, even if the host-address
 * specifiers are different.
 *
 * TCP services without official port numbers are handled with the
 * RFC1078-based tcpmux internal service. Tcpmux listens on port 1 for
 * requests. When a connection is made from a foreign host, the service
 * requested is passed to tcpmux, which looks it up in the servtab list
 * and returns the proper entry for the service. Tcpmux returns a
 * negative reply if the service doesn't exist, otherwise the invoked
 * server is expected to return the positive reply if the service type in
 * inetd.conf file has the prefix "tcpmux/". If the service type has the
 * prefix "tcpmux/+", tcpmux will return the positive reply for the
 * process; this is for compatibility with older server code, and also
 * allows you to invoke programs that use stdin/stdout without putting any
 * special server code in them. Services that use tcpmux are "nowait"
 * because they do not have a well-known port and hence cannot listen
 * for new requests.
 *
 * Comment lines are indicated by a `#' in column 1.
 *
 * #ifdef IPSEC
 * Comment lines that start with "#@" denote IPsec policy string, as described
 * in ipsec_set_policy(3).  This will affect all the following items in
 * inetd.conf(8).  To reset the policy, just use "#@" line.  By default,
 * there's no IPsec policy.
 * #endif
 */

/*
 * Here's the scoop concerning the user:group feature:
 *
 * 1) set-group-option off.
 *
 * 	a) user = root:	NO setuid() or setgid() is done
 *
 * 	b) other:	setuid()
 * 			setgid(primary group as found in passwd)
 * 			initgroups(name, primary group)
 *
 * 2) set-group-option on.
 *
 * 	a) user = root:	NO setuid()
 * 			setgid(specified group)
 * 			NO initgroups()
 *
 * 	b) other:	setuid()
 * 			setgid(specified group)
 * 			initgroups(name, specified group)
 *
 */

#include <sys/param.h>
#include <sys/stat.h>
#include <sys/ioctl.h>
#include <sys/socket.h>
#include <sys/un.h>
#include <sys/wait.h>
#include <sys/time.h>
#include <sys/resource.h>
#include <sys/event.h>

#ifndef NO_RPC
#define RPC
#endif

#include <net/if.h>

#include <netinet/in.h>
#include <arpa/inet.h>
#ifdef RPC
#include <rpc/rpc.h>
#include <rpc/rpcb_clnt.h>
#include <netconfig.h>
#endif

#include <ctype.h>
#include <errno.h>
#include <fcntl.h>
#include <grp.h>
#include <netdb.h>
#include <pwd.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <syslog.h>
#include <unistd.h>
#include <util.h>
#include <ifaddrs.h>

#include "pathnames.h"

#ifdef IPSEC
#include <netipsec/ipsec.h>
#ifndef IPSEC_POLICY_IPSEC	/* no ipsec support on old ipsec */
#undef IPSEC
#endif
#include "ipsec.h"
#endif

#ifdef LIBWRAP
# include <tcpd.h>
#ifndef LIBWRAP_ALLOW_FACILITY
# define LIBWRAP_ALLOW_FACILITY LOG_AUTH
#endif
#ifndef LIBWRAP_ALLOW_SEVERITY
# define LIBWRAP_ALLOW_SEVERITY LOG_INFO
#endif
#ifndef LIBWRAP_DENY_FACILITY
# define LIBWRAP_DENY_FACILITY LOG_AUTH
#endif
#ifndef LIBWRAP_DENY_SEVERITY
# define LIBWRAP_DENY_SEVERITY LOG_WARNING
#endif
int allow_severity = LIBWRAP_ALLOW_FACILITY|LIBWRAP_ALLOW_SEVERITY;
int deny_severity = LIBWRAP_DENY_FACILITY|LIBWRAP_DENY_SEVERITY;
#endif

#define	TOOMANY		40		/* don't start more than TOOMANY */
#define	CNT_INTVL	60		/* servers in CNT_INTVL sec. */
#define	RETRYTIME	(60*10)		/* retry after bind or server fail */

#define	A_CNT(a)	(sizeof (a) / sizeof (a[0]))

int	debug;
#ifdef LIBWRAP
int	lflag;
#endif
int	maxsock;
#ifndef __minix
int	kq;
#else /* __minix */
int	sig_pipe[2];
sigset_t sig_mask, old_mask;
#endif /* __minix */
int	options;
int	timingout;
const int niflags = NI_NUMERICHOST | NI_NUMERICSERV;

#ifndef OPEN_MAX
#define OPEN_MAX	64
#endif

/* Reserve some descriptors, 3 stdio + at least: 1 log, 1 conf. file */
#define FD_MARGIN	(8)
rlim_t		rlim_ofile_cur = OPEN_MAX;

struct rlimit	rlim_ofile;

struct kevent	changebuf[64];
size_t		changes;

struct	servtab {
	char	*se_hostaddr;		/* host address to listen on */
	char	*se_service;		/* name of service */
	int	se_socktype;		/* type of socket to use */
	int	se_family;		/* address family */
	char	*se_proto;		/* protocol used */
	int	se_sndbuf;		/* sndbuf size */
	int	se_rcvbuf;		/* rcvbuf size */
	int	se_rpcprog;		/* rpc program number */
	int	se_rpcversl;		/* rpc program lowest version */
	int	se_rpcversh;		/* rpc program highest version */
#define isrpcservice(sep)	((sep)->se_rpcversl != 0)
	pid_t	se_wait;		/* single threaded server */
	short	se_checked;		/* looked at during merge */
	char	*se_user;		/* user name to run as */
	char	*se_group;		/* group name to run as */
	struct	biltin *se_bi;		/* if built-in, description */
	char	*se_server;		/* server program */
#define	MAXARGV 20
	char	*se_argv[MAXARGV+1];	/* program arguments */
#ifdef IPSEC
	char	*se_policy;		/* IPsec poilcy string */
#endif
	struct accept_filter_arg se_accf; /* accept filter for stream service */
	int	se_fd;			/* open descriptor */
	int	se_type;		/* type */
	union {
		struct	sockaddr se_un_ctrladdr;
		struct	sockaddr_in se_un_ctrladdr_in;
		struct	sockaddr_in6 se_un_ctrladdr_in6;
		struct	sockaddr_un se_un_ctrladdr_un;
	} se_un;			/* bound address */
#define se_ctrladdr	se_un.se_un_ctrladdr
#define se_ctrladdr_in	se_un.se_un_ctrladdr_in
#define se_ctrladdr_un	se_un.se_un_ctrladdr_un
	int	se_ctrladdr_size;
	int	se_max;			/* max # of instances of this service */
	int	se_count;		/* number started since se_time */
	struct	timeval se_time;	/* start of se_count */
	struct	servtab *se_next;
} *servtab;

#define NORM_TYPE	0
#define MUX_TYPE	1
#define MUXPLUS_TYPE	2
#define FAITH_TYPE	3
#define ISMUX(sep)	(((sep)->se_type == MUX_TYPE) || \
			 ((sep)->se_type == MUXPLUS_TYPE))
#define ISMUXPLUS(sep)	((sep)->se_type == MUXPLUS_TYPE)


static void	chargen_dg(int, struct servtab *);
static void	chargen_stream(int, struct servtab *);
static void	close_sep(struct servtab *);
static void	config(void);
static void	daytime_dg(int, struct servtab *);
static void	daytime_stream(int, struct servtab *);
static void	discard_dg(int, struct servtab *);
static void	discard_stream(int, struct servtab *);
static void	echo_dg(int, struct servtab *);
static void	echo_stream(int, struct servtab *);
static void	endconfig(void);
static struct servtab *enter(struct servtab *);
static void	freeconfig(struct servtab *);
static struct servtab *getconfigent(void);
__dead static void	goaway(void);
static void	machtime_dg(int, struct servtab *);
static void	machtime_stream(int, struct servtab *);
static char    *newstr(const char *);
static char    *nextline(FILE *);
static void	print_service(const char *, struct servtab *);
static void	reapchild(void);
static void	retry(void);
static void	run_service(int, struct servtab *, int);
static int	setconfig(void);
static void	setup(struct servtab *);
static char    *sskip(char **);
static char    *skip(char **);
static void	tcpmux(int, struct servtab *);
__dead static void	usage(void);
static void	register_rpc(struct servtab *);
static void	unregister_rpc(struct servtab *);
static void	bump_nofile(void);
static void	inetd_setproctitle(char *, int);
static void	initring(void);
static uint32_t	machtime(void);
static int	port_good_dg(struct sockaddr *);
static int 	dg_broadcast(struct in_addr *);
#ifndef __minix
static int	my_kevent(const struct kevent *, size_t, struct kevent *,
		size_t);
static struct kevent *	allocchange(void);
#endif /* !__minix */
static int	get_line(int, char *, int);
static void	spawn(struct servtab *, int);

struct biltin {
	const char *bi_service;		/* internally provided service name */
	int	bi_socktype;		/* type of socket supported */
	short	bi_fork;		/* 1 if should fork before call */
	short	bi_wait;		/* 1 if should wait for child */
	void	(*bi_fn)(int, struct servtab *);
					/* function which performs it */
} biltins[] = {
	/* Echo received data */
	{ "echo",	SOCK_STREAM,	1, 0,	echo_stream },
	{ "echo",	SOCK_DGRAM,	0, 0,	echo_dg },

	/* Internet /dev/null */
	{ "discard",	SOCK_STREAM,	1, 0,	discard_stream },
	{ "discard",	SOCK_DGRAM,	0, 0,	discard_dg },

	/* Return 32 bit time since 1970 */
	{ "time",	SOCK_STREAM,	0, 0,	machtime_stream },
	{ "time",	SOCK_DGRAM,	0, 0,	machtime_dg },

	/* Return human-readable time */
	{ "daytime",	SOCK_STREAM,	0, 0,	daytime_stream },
	{ "daytime",	SOCK_DGRAM,	0, 0,	daytime_dg },

	/* Familiar character generator */
	{ "chargen",	SOCK_STREAM,	1, 0,	chargen_stream },
	{ "chargen",	SOCK_DGRAM,	0, 0,	chargen_dg },

	{ "tcpmux",	SOCK_STREAM,	1, 0,	tcpmux },

	{ NULL, 0, 0, 0, NULL }
};

/* list of "bad" ports. I.e. ports that are most obviously used for
 * "cycling packets" denial of service attacks. See /etc/services.
 * List must end with port number "0".
 */

u_int16_t bad_ports[] =  { 7, 9, 13, 19, 37, 0 };


#define NUMINT	(sizeof(intab) / sizeof(struct inent))
const char	*CONFIG = _PATH_INETDCONF;

static int my_signals[] =
    { SIGALRM, SIGHUP, SIGCHLD, SIGTERM, SIGINT, SIGPIPE };

#ifdef __minix
/*
 * NetBSD uses kqueue to catch signals, while (explicitly) ignoring them at the
 * process level.  We (MINIX3) do catch the signals at the process level,
 * instead sending them into select() using a pipe (djb's self-pipe trick).
 * That is safe, except it may interrupt system calls other than our select(),
 * so we also have to set appropriate signal masks (clearing them upon fork).
 */
static void
got_signal(int sig)
{

	(void) write(sig_pipe[1], &sig, sizeof(sig));
}
#endif /* __minix */

int
main(int argc, char *argv[])
{
	int		ch, n, reload = 1;

	while ((ch = getopt(argc, argv,
#ifdef LIBWRAP
					"dl"
#else
					"d"
#endif
					   )) != -1)
		switch(ch) {
		case 'd':
			debug = 1;
			options |= SO_DEBUG;
			break;
#ifdef LIBWRAP
		case 'l':
			lflag = 1;
			break;
#endif
		case '?':
		default:
			usage();
		}
	argc -= optind;
	argv += optind;

	if (argc > 0)
		CONFIG = argv[0];

	if (!debug)
		daemon(0, 0);
	openlog("inetd", LOG_PID | LOG_NOWAIT, LOG_DAEMON);
	pidfile(NULL);

#ifndef __minix
	kq = kqueue();
	if (kq < 0) {
		syslog(LOG_ERR, "kqueue: %m");
		return (EXIT_FAILURE);
	}
#else /* __minix */
	if (pipe2(sig_pipe, O_CLOEXEC | O_NONBLOCK) != 0) {
		syslog(LOG_ERR, "pipe2: %m");
		return (EXIT_FAILURE);
	}

	/* Block all signals until the first select() call.. just easier. */
	sigfillset(&sig_mask);
	(void) sigprocmask(SIG_SETMASK, &sig_mask, &old_mask);
	sig_mask = old_mask;
#endif /* __minix */

	if (getrlimit(RLIMIT_NOFILE, &rlim_ofile) < 0) {
		syslog(LOG_ERR, "getrlimit: %m");
	} else {
		rlim_ofile_cur = rlim_ofile.rlim_cur;
		if (rlim_ofile_cur == RLIM_INFINITY)	/* ! */
			rlim_ofile_cur = OPEN_MAX;
	}

	for (n = 0; n < (int)A_CNT(my_signals); n++) {
		int	signum;

		signum = my_signals[n];
#ifndef __minix
		if (signum != SIGCHLD)
			(void) signal(signum, SIG_IGN);

		if (signum != SIGPIPE) {
			struct kevent	*ev;

			ev = allocchange();
			EV_SET(ev, signum, EVFILT_SIGNAL, EV_ADD | EV_ENABLE,
			    0, 0, 0);
		}
#else /* __minix */
		/* The above code ignores but does not "catch" SIGPIPE. */
		if (signum != SIGPIPE)
			(void) signal(signum, got_signal);
		else
			(void) signal(signum, SIG_IGN);
		sigaddset(&sig_mask, signum);
#endif /* __minix */
	}

	for (;;) {
		int		ctrl;
#ifndef __minix
		struct kevent	eventbuf[64], *ev;
#else
		fd_set fds;
		int sig, highfd;
#endif /* !__minix */
		struct servtab	*sep;

		if (reload) {
			reload = 0;
			config();
		}

#ifdef __minix
		FD_ZERO(&fds);
		FD_SET(sig_pipe[0], &fds);
		highfd = sig_pipe[0];

		for (sep = servtab; sep != NULL; sep = sep->se_next)
			if (sep->se_fd != -1 && (unsigned)sep->se_wait <= 1) {
				FD_SET(sep->se_fd, &fds);
				if (highfd < sep->se_fd)
					highfd = sep->se_fd;
			}

		/*
		 * Unblock all the signals we want to catch for the duration of
		 * the select() call.  We do not yet have pselect(), but the
		 * lack of atomicity does not affect correctness here, because
		 * all the signals go through the pipe anyway--that is also why
		 * we reissue the select() even if we did catch a signal.
		 */
		(void) sigprocmask(SIG_SETMASK, &old_mask, NULL);

		while (select(highfd + 1, &fds, NULL, NULL, NULL) == -1 &&
		    errno == EINTR);

		(void) sigprocmask(SIG_SETMASK, &sig_mask, NULL);

		if (FD_ISSET(sig_pipe[0], &fds)) {
			while (read(sig_pipe[0], &sig, sizeof(sig)) != -1) {
				switch (sig) {
#else /* !__minix */
		n = my_kevent(changebuf, changes, eventbuf, A_CNT(eventbuf));
		changes = 0;

		for (ev = eventbuf; n > 0; ev++, n--) {
			if (ev->filter == EVFILT_SIGNAL) {
				switch (ev->ident) {
#endif /* !__minix */
				case SIGALRM:
					retry();
					break;
				case SIGCHLD:
					reapchild();
					break;
				case SIGTERM:
				case SIGINT:
					goaway();
					break;
				case SIGHUP:
					reload = 1;
					break;
				}
				continue;
			}
#ifdef __minix
		}

		for (sep = servtab; sep != NULL; sep = sep->se_next) {
			if (sep->se_fd == -1 || (unsigned)sep->se_wait > 1 ||
			    !FD_ISSET(sep->se_fd, &fds))
				continue;
#else /* !__minix */
			if (ev->filter != EVFILT_READ)
				continue;
			sep = (struct servtab *)ev->udata;
			/* Paranoia */
			if ((int)ev->ident != sep->se_fd)
				continue;
#endif /* !__minix */
			if (debug)
				fprintf(stderr, "someone wants %s\n",
				    sep->se_service);
			if (!sep->se_wait && sep->se_socktype == SOCK_STREAM) {
				/* XXX here do the libwrap check-before-accept*/
				ctrl = accept(sep->se_fd, NULL, NULL);
				if (debug)
					fprintf(stderr, "accept, ctrl %d\n",
					    ctrl);
				if (ctrl < 0) {
					if (errno != EINTR)
						syslog(LOG_WARNING,
						    "accept (for %s): %m",
						    sep->se_service);
					continue;
				}
			} else
				ctrl = sep->se_fd;
			spawn(sep, ctrl);
		}
	}
}

static void
spawn(struct servtab *sep, int ctrl)
{
	int dofork;
	pid_t pid;

	pid = 0;
#ifdef LIBWRAP_INTERNAL
	dofork = 1;
#else
	dofork = (sep->se_bi == 0 || sep->se_bi->bi_fork);
#endif
	if (dofork) {
		if (sep->se_count++ == 0)
			(void)gettimeofday(&sep->se_time, NULL);
		else if (sep->se_count >= sep->se_max) {
			struct timeval now;

			(void)gettimeofday(&now, NULL);
			if (now.tv_sec - sep->se_time.tv_sec > CNT_INTVL) {
				sep->se_time = now;
				sep->se_count = 1;
			} else {
				syslog(LOG_ERR,
				    "%s/%s max spawn rate (%d in %d seconds) "
				    "exceeded; service not started",
				    sep->se_service, sep->se_proto,
				    sep->se_max, CNT_INTVL);
				if (!sep->se_wait && sep->se_socktype ==
				    SOCK_STREAM)
					close(ctrl);
				close_sep(sep);
				if (!timingout) {
					timingout = 1;
					alarm(RETRYTIME);
				}
				return;
			}
		}
		pid = fork();
		if (pid < 0) {
			syslog(LOG_ERR, "fork: %m");
			if (!sep->se_wait && sep->se_socktype == SOCK_STREAM)
				close(ctrl);
			sleep(1);
			return;
		}
		if (pid != 0 && sep->se_wait) {
#ifndef __minix
			struct kevent	*ev;

			sep->se_wait = pid;
			ev = allocchange();
			EV_SET(ev, sep->se_fd, EVFILT_READ,
			    EV_DELETE, 0, 0, 0);
#endif /* !__minix */
		}
		if (pid == 0) {
			size_t	n;

			for (n = 0; n < A_CNT(my_signals); n++)
				(void) signal(my_signals[n], SIG_DFL);
#ifdef __minix
			close(sig_pipe[0]);
			close(sig_pipe[1]);

			(void) sigprocmask(SIG_SETMASK, &old_mask, NULL);
#endif /* __minix */
			if (debug)
				setsid();
		}
	}
	if (pid == 0) {
		run_service(ctrl, sep, dofork);
		if (dofork)
			exit(0);
	}
	if (!sep->se_wait && sep->se_socktype == SOCK_STREAM)
		close(ctrl);
}

static void
run_service(int ctrl, struct servtab *sep, int didfork)
{
	struct passwd *pwd;
	struct group *grp = NULL;	/* XXX gcc */
	char buf[NI_MAXSERV];
	struct servtab *s;
#ifdef LIBWRAP
	char abuf[BUFSIZ];
	struct request_info req;
	int denied;
	char *service = NULL;	/* XXX gcc */
#endif

#ifdef LIBWRAP
#ifndef LIBWRAP_INTERNAL
	if (sep->se_bi == 0)
#endif
	if (!sep->se_wait && sep->se_socktype == SOCK_STREAM) {
		request_init(&req, RQ_DAEMON, sep->se_argv[0] ?
		    sep->se_argv[0] : sep->se_service, RQ_FILE, ctrl, NULL);
		fromhost(&req);
		denied = !hosts_access(&req);
		if (denied || lflag) {
			if (getnameinfo(&sep->se_ctrladdr,
			    (socklen_t)sep->se_ctrladdr.sa_len, NULL, 0,
			    buf, sizeof(buf), 0) != 0) {
				/* shouldn't happen */
				(void)snprintf(buf, sizeof buf, "%d",
				    ntohs(sep->se_ctrladdr_in.sin_port));
			}
			service = buf;
			if (req.client->sin) {
				sockaddr_snprintf(abuf, sizeof(abuf), "%a",
				    req.client->sin);
			} else {
				strcpy(abuf, "(null)");
			}
		}
		if (denied) {
			syslog(deny_severity,
			    "refused connection from %.500s(%s), service %s (%s)",
			    eval_client(&req), abuf, service, sep->se_proto);
			goto reject;
		}
		if (lflag) {
			syslog(allow_severity,
			    "connection from %.500s(%s), service %s (%s)",
			    eval_client(&req), abuf, service, sep->se_proto);
		}
	}
#endif /* LIBWRAP */

	if (sep->se_bi) {
		if (didfork) {
			for (s = servtab; s; s = s->se_next)
				if (s->se_fd != -1 && s->se_fd != ctrl) {
					close(s->se_fd);
					s->se_fd = -1;
				}
		}
		(*sep->se_bi->bi_fn)(ctrl, sep);
	} else {
		if ((pwd = getpwnam(sep->se_user)) == NULL) {
			syslog(LOG_ERR, "%s/%s: %s: No such user",
			    sep->se_service, sep->se_proto, sep->se_user);
			goto reject;
		}
		if (sep->se_group &&
		    (grp = getgrnam(sep->se_group)) == NULL) {
			syslog(LOG_ERR, "%s/%s: %s: No such group",
			    sep->se_service, sep->se_proto, sep->se_group);
			goto reject;
		}
		if (pwd->pw_uid) {
			if (sep->se_group)
				pwd->pw_gid = grp->gr_gid;
			if (setgid(pwd->pw_gid) < 0) {
				syslog(LOG_ERR,
				 "%s/%s: can't set gid %d: %m", sep->se_service,
				    sep->se_proto, pwd->pw_gid);
				goto reject;
			}
			(void) initgroups(pwd->pw_name,
			    pwd->pw_gid);
			if (setuid(pwd->pw_uid) < 0) {
				syslog(LOG_ERR,
				 "%s/%s: can't set uid %d: %m", sep->se_service,
				    sep->se_proto, pwd->pw_uid);
				goto reject;
			}
		} else if (sep->se_group) {
			(void) setgid((gid_t)grp->gr_gid);
		}
		if (debug)
			fprintf(stderr, "%d execl %s\n",
			    getpid(), sep->se_server);
		/* Set our control descriptor to not close-on-exec... */
		if (fcntl(ctrl, F_SETFD, 0) < 0)
			syslog(LOG_ERR, "fcntl (%d, F_SETFD, 0): %m", ctrl);
		/* ...and dup it to stdin, stdout, and stderr. */
		if (ctrl != 0) {
			dup2(ctrl, 0);
			close(ctrl);
			ctrl = 0;
		}
		dup2(0, 1);
		dup2(0, 2);
		if (rlim_ofile.rlim_cur != rlim_ofile_cur &&
		    setrlimit(RLIMIT_NOFILE, &rlim_ofile) < 0)
			syslog(LOG_ERR, "setrlimit: %m");
		execv(sep->se_server, sep->se_argv);
		syslog(LOG_ERR, "cannot execute %s: %m", sep->se_server);
	reject:
		if (sep->se_socktype != SOCK_STREAM)
			recv(ctrl, buf, sizeof (buf), 0);
		_exit(1);
	}
}

static void
reapchild(void)
{
	int status;
	pid_t pid;
	struct servtab *sep;

	for (;;) {
		pid = wait3(&status, WNOHANG, NULL);
		if (pid <= 0)
			break;
		if (debug)
			(void) fprintf(stderr, "%d reaped, status %#x\n", 
			    pid, status);
		for (sep = servtab; sep != NULL; sep = sep->se_next)
			if (sep->se_wait == pid) {
#ifndef __minix
				struct kevent	*ev;
#endif /* !__minix */

				if (WIFEXITED(status) && WEXITSTATUS(status))
					syslog(LOG_WARNING,
					    "%s: exit status %u",
					    sep->se_server, WEXITSTATUS(status));
				else if (WIFSIGNALED(status))
					syslog(LOG_WARNING,
					    "%s: exit signal %u",
					    sep->se_server, WTERMSIG(status));
				sep->se_wait = 1;
#ifndef __minix
				ev = allocchange();
				EV_SET(ev, sep->se_fd, EVFILT_READ,
				    EV_ADD | EV_ENABLE, 0, 0, (intptr_t)sep);
#endif /* !__minix */
				if (debug)
					fprintf(stderr, "restored %s, fd %d\n",
					    sep->se_service, sep->se_fd);
			}
	}
}

static void
config(void)
{
	struct servtab *sep, *cp, **sepp;
	size_t n;

	if (!setconfig()) {
		syslog(LOG_ERR, "%s: %m", CONFIG);
		return;
	}
	for (sep = servtab; sep != NULL; sep = sep->se_next)
		sep->se_checked = 0;
	while ((cp = getconfigent()) != NULL) {
		for (sep = servtab; sep != NULL; sep = sep->se_next)
			if (strcmp(sep->se_service, cp->se_service) == 0 &&
			    strcmp(sep->se_hostaddr, cp->se_hostaddr) == 0 &&
			    strcmp(sep->se_proto, cp->se_proto) == 0 &&
			    ISMUX(sep) == ISMUX(cp))
				break;
		if (sep != NULL) {
			int i;

#define SWAP(type, a, b) {type c = a; a = b; b = c;}

			/*
			 * sep->se_wait may be holding the pid of a daemon
			 * that we're waiting for.  If so, don't overwrite
			 * it unless the config file explicitly says don't
			 * wait.
			 */
			if (cp->se_bi == 0 &&
			    (sep->se_wait == 1 || cp->se_wait == 0))
				sep->se_wait = cp->se_wait;
			SWAP(char *, sep->se_user, cp->se_user);
			SWAP(char *, sep->se_group, cp->se_group);
			SWAP(char *, sep->se_server, cp->se_server);
			for (i = 0; i < MAXARGV; i++)
				SWAP(char *, sep->se_argv[i], cp->se_argv[i]);
#ifdef IPSEC
			SWAP(char *, sep->se_policy, cp->se_policy);
#endif
			SWAP(int, cp->se_type, sep->se_type);
			SWAP(int, cp->se_max, sep->se_max);
#undef SWAP
			if (isrpcservice(sep))
				unregister_rpc(sep);
			sep->se_rpcversl = cp->se_rpcversl;
			sep->se_rpcversh = cp->se_rpcversh;
			freeconfig(cp);
			if (debug)
				print_service("REDO", sep);
		} else {
			sep = enter(cp);
			if (debug)
				print_service("ADD ", sep);
		}
		sep->se_checked = 1;

		switch (sep->se_family) {
		case AF_LOCAL:
			if (sep->se_fd != -1)
				break;
			n = strlen(sep->se_service);
			if (n >= sizeof(sep->se_ctrladdr_un.sun_path)) {
				syslog(LOG_ERR, "%s: address too long",
				    sep->se_service);
				sep->se_checked = 0;
				continue;
			}
			(void)unlink(sep->se_service);
			strlcpy(sep->se_ctrladdr_un.sun_path,
			    sep->se_service, n);
			sep->se_ctrladdr_un.sun_family = AF_LOCAL;
			sep->se_ctrladdr_size = (int)(n +
			    sizeof(sep->se_ctrladdr_un) -
			    sizeof(sep->se_ctrladdr_un.sun_path));
			if (!ISMUX(sep))
				setup(sep);
			break;
		case AF_INET:
#ifdef INET6
		case AF_INET6:
#endif
		    {
			struct addrinfo hints, *res;
			char *host;
			const char *port;
			int error;
			int s;

			/* check if the family is supported */
			s = socket(sep->se_family, SOCK_DGRAM, 0);
			if (s < 0) {
				syslog(LOG_WARNING,
				    "%s/%s: %s: the address family is not "
				    "supported by the kernel",
				    sep->se_service, sep->se_proto,
				    sep->se_hostaddr);
				sep->se_checked = 0;
				continue;
			}
			close(s);

			memset(&hints, 0, sizeof(hints));
			hints.ai_family = sep->se_family;
			hints.ai_socktype = sep->se_socktype;
			hints.ai_flags = AI_PASSIVE;
			if (!strcmp(sep->se_hostaddr, "*"))
				host = NULL;
			else
				host = sep->se_hostaddr;
			if (isrpcservice(sep) || ISMUX(sep))
				port = "0";
			else
				port = sep->se_service;
			error = getaddrinfo(host, port, &hints, &res);
			if (error) {
				if (error == EAI_SERVICE) {
					/* gai_strerror not friendly enough */
					syslog(LOG_WARNING, "%s/%s: "
					    "unknown service",
					    sep->se_service, sep->se_proto);
				} else {
					syslog(LOG_ERR, "%s/%s: %s: %s",
					    sep->se_service, sep->se_proto,
					    sep->se_hostaddr,
					    gai_strerror(error));
				}
				sep->se_checked = 0;
				continue;
			}
			if (res->ai_next) {
				syslog(LOG_ERR,
					"%s/%s: %s: resolved to multiple addr",
				    sep->se_service, sep->se_proto,
				    sep->se_hostaddr);
				sep->se_checked = 0;
				freeaddrinfo(res);
				continue;
			}
			memcpy(&sep->se_ctrladdr, res->ai_addr,
				res->ai_addrlen);
			if (ISMUX(sep)) {
				sep->se_fd = -1;
				freeaddrinfo(res);
				continue;
			}
			sep->se_ctrladdr_size = res->ai_addrlen;
			freeaddrinfo(res);
#ifdef RPC
			if (isrpcservice(sep)) {
				struct rpcent *rp;

				sep->se_rpcprog = atoi(sep->se_service);
				if (sep->se_rpcprog == 0) {
					rp = getrpcbyname(sep->se_service);
					if (rp == 0) {
						syslog(LOG_ERR,
						    "%s/%s: unknown service",
						    sep->se_service,
						    sep->se_proto);
						sep->se_checked = 0;
						continue;
					}
					sep->se_rpcprog = rp->r_number;
				}
				if (sep->se_fd == -1 && !ISMUX(sep))
					setup(sep);
				if (sep->se_fd != -1)
					register_rpc(sep);
			} else
#endif
			{
				if (sep->se_fd >= 0)
					close_sep(sep);
				if (sep->se_fd == -1 && !ISMUX(sep))
					setup(sep);
			}
		    }
		}
	}
	endconfig();
	/*
	 * Purge anything not looked at above.
	 */
	sepp = &servtab;
	while ((sep = *sepp) != NULL) {
		if (sep->se_checked) {
			sepp = &sep->se_next;
			continue;
		}
		*sepp = sep->se_next;
		if (sep->se_fd >= 0)
			close_sep(sep);
		if (isrpcservice(sep))
			unregister_rpc(sep);
		if (sep->se_family == AF_LOCAL)
			(void)unlink(sep->se_service);
		if (debug)
			print_service("FREE", sep);
		freeconfig(sep);
		free(sep);
	}
}

static void
retry(void)
{
	struct servtab *sep;

	timingout = 0;
	for (sep = servtab; sep != NULL; sep = sep->se_next) {
		if (sep->se_fd == -1 && !ISMUX(sep)) {
			switch (sep->se_family) {
			case AF_LOCAL:
			case AF_INET:
#ifdef INET6
			case AF_INET6:
#endif
				setup(sep);
				if (sep->se_fd >= 0 && isrpcservice(sep))
					register_rpc(sep);
				break;
			}
		}
	}
}

static void
goaway(void)
{
	struct servtab *sep;

	for (sep = servtab; sep != NULL; sep = sep->se_next) {
		if (sep->se_fd == -1)
			continue;

		switch (sep->se_family) {
		case AF_LOCAL:
			(void)unlink(sep->se_service);
			break;
		case AF_INET:
#ifdef INET6
		case AF_INET6:
#endif
			if (sep->se_wait == 1 && isrpcservice(sep))
				unregister_rpc(sep);
			break;
		}
		(void)close(sep->se_fd);
		sep->se_fd = -1;
	}
	exit(0);
}

static void
setup(struct servtab *sep)
{
	int		on = 1;
#ifdef INET6
	int		off = 0;
#endif
#ifndef __minix
	struct kevent	*ev;
#endif /* !__minix */

	if ((sep->se_fd = socket(sep->se_family, sep->se_socktype, 0)) < 0) {
		if (debug)
			fprintf(stderr, "socket failed on %s/%s: %s\n", 
			    sep->se_service, sep->se_proto, strerror(errno));
		syslog(LOG_ERR, "%s/%s: socket: %m",
		    sep->se_service, sep->se_proto);
		return;
	}
	/* Set all listening sockets to close-on-exec. */
	if (fcntl(sep->se_fd, F_SETFD, FD_CLOEXEC) < 0) {
		syslog(LOG_ERR, "%s/%s: fcntl(F_SETFD, FD_CLOEXEC): %m",
		    sep->se_service, sep->se_proto);
		close(sep->se_fd);
		sep->se_fd = -1;
		return;
	}

#define	turnon(fd, opt) \
setsockopt(fd, SOL_SOCKET, opt, &on, (socklen_t)sizeof(on))
	if (strcmp(sep->se_proto, "tcp") == 0 && (options & SO_DEBUG) &&
	    turnon(sep->se_fd, SO_DEBUG) < 0)
		syslog(LOG_ERR, "setsockopt (SO_DEBUG): %m");
	if (turnon(sep->se_fd, SO_REUSEADDR) < 0)
		syslog(LOG_ERR, "setsockopt (SO_REUSEADDR): %m");
#undef turnon

	/* Set the socket buffer sizes, if specified. */
	if (sep->se_sndbuf != 0 && setsockopt(sep->se_fd, SOL_SOCKET,
	    SO_SNDBUF, &sep->se_sndbuf, (socklen_t)sizeof(sep->se_sndbuf)) < 0)
		syslog(LOG_ERR, "setsockopt (SO_SNDBUF %d): %m",
		    sep->se_sndbuf);
	if (sep->se_rcvbuf != 0 && setsockopt(sep->se_fd, SOL_SOCKET,
	    SO_RCVBUF, &sep->se_rcvbuf, (socklen_t)sizeof(sep->se_rcvbuf)) < 0)
		syslog(LOG_ERR, "setsockopt (SO_RCVBUF %d): %m",
		    sep->se_rcvbuf);
#ifdef INET6
	if (sep->se_family == AF_INET6) {
		int *v;
		v = (sep->se_type == FAITH_TYPE) ? &on : &off;
		if (setsockopt(sep->se_fd, IPPROTO_IPV6, IPV6_FAITH,
		    v, (socklen_t)sizeof(*v)) < 0)
			syslog(LOG_ERR, "setsockopt (IPV6_FAITH): %m");
	}
#endif
#ifdef IPSEC
	if (ipsecsetup(sep->se_family, sep->se_fd, sep->se_policy) < 0 &&
	    sep->se_policy) {
		syslog(LOG_ERR, "%s/%s: ipsec setup failed",
		    sep->se_service, sep->se_proto);
		(void)close(sep->se_fd);
		sep->se_fd = -1;
		return;
	}
#endif

	if (bind(sep->se_fd, &sep->se_ctrladdr,
	    (socklen_t)sep->se_ctrladdr_size) < 0) {
		if (debug)
			fprintf(stderr, "bind failed on %s/%s: %s\n",
			    sep->se_service, sep->se_proto, strerror(errno));
		syslog(LOG_ERR, "%s/%s: bind: %m",
		    sep->se_service, sep->se_proto);
		(void) close(sep->se_fd);
		sep->se_fd = -1;
		if (!timingout) {
			timingout = 1;
			alarm(RETRYTIME);
		}
		return;
	}
	if (sep->se_socktype == SOCK_STREAM)
		listen(sep->se_fd, 10);

	/* Set the accept filter, if specified. To be done after listen.*/
	if (sep->se_accf.af_name[0] != 0 && setsockopt(sep->se_fd, SOL_SOCKET,
	    SO_ACCEPTFILTER, &sep->se_accf,
	    (socklen_t)sizeof(sep->se_accf)) < 0)
		syslog(LOG_ERR, "setsockopt(SO_ACCEPTFILTER %s): %m",
		    sep->se_accf.af_name);

#ifndef __minix
	ev = allocchange();
	EV_SET(ev, sep->se_fd, EVFILT_READ, EV_ADD | EV_ENABLE, 0, 0,
	    (intptr_t)sep);
#endif /* !__minix */
	if (sep->se_fd > maxsock) {
		maxsock = sep->se_fd;
		if (maxsock > (int)(rlim_ofile_cur - FD_MARGIN))
			bump_nofile();
	}
	if (debug)
		fprintf(stderr, "registered %s on %d\n",
		    sep->se_server, sep->se_fd);
}

/*
 * Finish with a service and its socket.
 */
static void
close_sep(struct servtab *sep)
{
	if (sep->se_fd >= 0) {
		(void) close(sep->se_fd);
		sep->se_fd = -1;
	}
	sep->se_count = 0;
}

static void
register_rpc(struct servtab *sep)
{
#ifdef RPC
	struct netbuf nbuf;
	struct sockaddr_storage ss;
	struct netconfig *nconf;
	socklen_t socklen;
	int n;

	if ((nconf = getnetconfigent(sep->se_proto+4)) == NULL) {
		syslog(LOG_ERR, "%s: getnetconfigent failed",
		    sep->se_proto);
		return;
	}
	socklen = sizeof ss;
	if (getsockname(sep->se_fd, (struct sockaddr *)(void *)&ss, &socklen) < 0) {
		syslog(LOG_ERR, "%s/%s: getsockname: %m",
		    sep->se_service, sep->se_proto);
		return;
	}

	nbuf.buf = &ss;
	nbuf.len = ss.ss_len;
	nbuf.maxlen = sizeof (struct sockaddr_storage);
	for (n = sep->se_rpcversl; n <= sep->se_rpcversh; n++) {
		if (debug)
			fprintf(stderr, "rpcb_set: %u %d %s %s\n",
			    sep->se_rpcprog, n, nconf->nc_netid,
			    taddr2uaddr(nconf, &nbuf));
		(void)rpcb_unset((unsigned int)sep->se_rpcprog, (unsigned int)n, nconf);
		if (!rpcb_set((unsigned int)sep->se_rpcprog, (unsigned int)n, nconf, &nbuf))
			syslog(LOG_ERR, "rpcb_set: %u %d %s %s%s",
			    sep->se_rpcprog, n, nconf->nc_netid,
			    taddr2uaddr(nconf, &nbuf), clnt_spcreateerror(""));
	}
#endif /* RPC */
}

static void
unregister_rpc(struct servtab *sep)
{
#ifdef RPC
	int n;
	struct netconfig *nconf;

	if ((nconf = getnetconfigent(sep->se_proto+4)) == NULL) {
		syslog(LOG_ERR, "%s: getnetconfigent failed",
		    sep->se_proto);
		return;
	}

	for (n = sep->se_rpcversl; n <= sep->se_rpcversh; n++) {
		if (debug)
			fprintf(stderr, "rpcb_unset(%u, %d, %s)\n",
			    sep->se_rpcprog, n, nconf->nc_netid);
		if (!rpcb_unset((unsigned int)sep->se_rpcprog, (unsigned int)n, nconf))
			syslog(LOG_ERR, "rpcb_unset(%u, %d, %s) failed\n",
			    sep->se_rpcprog, n, nconf->nc_netid);
	}
#endif /* RPC */
}


static struct servtab *
enter(struct servtab *cp)
{
	struct servtab *sep;

	sep = malloc(sizeof (*sep));
	if (sep == NULL) {
		syslog(LOG_ERR, "Out of memory.");
		exit(1);
	}
	*sep = *cp;
	sep->se_fd = -1;
	sep->se_rpcprog = -1;
	sep->se_next = servtab;
	servtab = sep;
	return (sep);
}

FILE	*fconfig = NULL;
struct	servtab serv;
char	line[LINE_MAX];
char    *defhost;
#ifdef IPSEC
static char *policy = NULL;
#endif

static int
setconfig(void)
{
	if (defhost)
		free(defhost);
	defhost = newstr("*");
#ifdef IPSEC
	if (policy)
		free(policy);
	policy = NULL;
#endif
	if (fconfig != NULL) {
		fseek(fconfig, 0L, SEEK_SET);
		return (1);
	}
	fconfig = fopen(CONFIG, "r");
	return (fconfig != NULL);
}

static void
endconfig(void)
{
	if (fconfig != NULL) {
		(void) fclose(fconfig);
		fconfig = NULL;
	}
	if (defhost != NULL) {
		free(defhost);
		defhost = NULL;
	}
}

static struct servtab *
getconfigent(void)
{
	struct servtab *sep = &serv;
	int argc, val;
	char *cp, *cp0, *arg, *buf0, *buf1, *sz0, *sz1;
	static char TCPMUX_TOKEN[] = "tcpmux/";
#define MUX_LEN		(sizeof(TCPMUX_TOKEN)-1)
	char *hostdelim;

more:
	while ((cp = nextline(fconfig)) != NULL) {
#ifdef IPSEC
		/* lines starting with #@ is not a comment, but the policy */
		if (cp[0] == '#' && cp[1] == '@') {
			char *p;
			for (p = cp + 2; p && *p && isspace((unsigned char)*p); p++)
				;
			if (*p == '\0') {
				if (policy)
					free(policy);
				policy = NULL;
			} else {
				if (ipsecsetup_test(p) < 0) {
					syslog(LOG_ERR,
						"%s: invalid ipsec policy \"%s\"",
						CONFIG, p);
					exit(1);
				} else {
					if (policy)
						free(policy);
					policy = newstr(p);
				}
			}
		}
#endif
		if (*cp == '#' || *cp == '\0')
			continue;
		break;
	}
	if (cp == NULL)
		return (NULL);
	/*
	 * clear the static buffer, since some fields (se_ctrladdr,
	 * for example) don't get initialized here.
	 */
	memset(sep, 0, sizeof *sep);
	arg = skip(&cp);
	if (cp == NULL) {
		/* got an empty line containing just blanks/tabs. */
		goto more;
	}
	/* Check for a host name. */
	hostdelim = strrchr(arg, ':');
	if (hostdelim) {
		*hostdelim = '\0';
		if (arg[0] == '[' && hostdelim > arg && hostdelim[-1] == ']') {
			hostdelim[-1] = '\0';
			sep->se_hostaddr = newstr(arg + 1);
		} else
			sep->se_hostaddr = newstr(arg);
		arg = hostdelim + 1;
		/*
		 * If the line is of the form `host:', then just change the
		 * default host for the following lines.
		 */
		if (*arg == '\0') {
			arg = skip(&cp);
			if (cp == NULL) {
				free(defhost);
				defhost = sep->se_hostaddr;
				goto more;
			}
		}
	} else
		sep->se_hostaddr = newstr(defhost);
	if (strncmp(arg, TCPMUX_TOKEN, MUX_LEN) == 0) {
		char *c = arg + MUX_LEN;
		if (*c == '+') {
			sep->se_type = MUXPLUS_TYPE;
			c++;
		} else
			sep->se_type = MUX_TYPE;
		sep->se_service = newstr(c);
	} else {
		sep->se_service = newstr(arg);
		sep->se_type = NORM_TYPE;
	}

	arg = sskip(&cp);
	if (strncmp(arg, "stream", sizeof("stream") - 1) == 0) {
		char *accf, *accf_arg;

		sep->se_socktype = SOCK_STREAM;

		/* one and only one accept filter */
		accf = strchr(arg, ':');	
		if (accf) {
	    		if (accf != strrchr(arg, ':') ||/* more than one */
	    		    *(accf + 1) == '\0') {	/* nothing beyond */
				sep->se_socktype = -1;
			} else {
				accf++;			/* skip delimiter */
				strlcpy(sep->se_accf.af_name, accf,
					sizeof(sep->se_accf.af_name));
				accf_arg = strchr(accf, ',');
				if (accf_arg) {	/* zero or one arg, no more */
					if (strrchr(accf, ',') != accf_arg) {
						sep->se_socktype = -1;
					} else {
						accf_arg++;
						strlcpy(sep->se_accf.af_arg,
							accf_arg,
							sizeof(sep->se_accf.af_arg));
					}
				}
			}
		}
	}
		
	else if (strcmp(arg, "dgram") == 0)
		sep->se_socktype = SOCK_DGRAM;
	else if (strcmp(arg, "rdm") == 0)
		sep->se_socktype = SOCK_RDM;
	else if (strcmp(arg, "seqpacket") == 0)
		sep->se_socktype = SOCK_SEQPACKET;
	else if (strcmp(arg, "raw") == 0)
		sep->se_socktype = SOCK_RAW;
	else
		sep->se_socktype = -1;

	arg = sskip(&cp);
	if (sep->se_type == NORM_TYPE &&
	    strncmp(arg, "faith/", strlen("faith/")) == 0) {
		arg += strlen("faith/");
		sep->se_type = FAITH_TYPE;
	}
	sep->se_proto = newstr(arg);

#define	MALFORMED(arg) \
do { \
	syslog(LOG_ERR, "%s: malformed buffer size option `%s'", \
	    sep->se_service, (arg)); \
	goto more; \
	/*NOTREACHED*/ \
} while (/*CONSTCOND*/0)

#define	GETVAL(arg) \
do { \
	if (!isdigit((unsigned char)*(arg))) \
		MALFORMED(arg); \
	val = (int)strtol((arg), &cp0, 10); \
	if (cp0 != NULL) { \
		if (cp0[1] != '\0') \
			MALFORMED((arg)); \
		if (cp0[0] == 'k') \
			val *= 1024; \
		if (cp0[0] == 'm') \
			val *= 1024 * 1024; \
	} \
	if (val < 1) { \
		syslog(LOG_ERR, "%s: invalid buffer size `%s'", \
		    sep->se_service, (arg)); \
		goto more; \
	} \
	/*NOTREACHED*/ \
} while (/*CONSTCOND*/0)

#define	ASSIGN(arg) \
do { \
	if (strcmp((arg), "sndbuf") == 0) \
		sep->se_sndbuf = val; \
	else if (strcmp((arg), "rcvbuf") == 0) \
		sep->se_rcvbuf = val; \
	else \
		MALFORMED((arg)); \
} while (/*CONSTCOND*/0)

	/*
	 * Extract the send and receive buffer sizes before parsing
	 * the protocol.
	 */
	sep->se_sndbuf = sep->se_rcvbuf = 0;
	buf0 = buf1 = sz0 = sz1 = NULL;
	if ((buf0 = strchr(sep->se_proto, ',')) != NULL) {
		/* Not meaningful for Tcpmux services. */
		if (ISMUX(sep)) {
			syslog(LOG_ERR, "%s: can't specify buffer sizes for "
			    "tcpmux services", sep->se_service);
			goto more;
		}

		/* Skip the , */
		*buf0++ = '\0';

		/* Check to see if another socket buffer size was specified. */
		if ((buf1 = strchr(buf0, ',')) != NULL) {
			/* Skip the , */
			*buf1++ = '\0';

			/* Make sure a 3rd one wasn't specified. */
			if (strchr(buf1, ',') != NULL) {
				syslog(LOG_ERR, "%s: too many buffer sizes",
				    sep->se_service);
				goto more;
			}

			/* Locate the size. */
			if ((sz1 = strchr(buf1, '=')) == NULL)
				MALFORMED(buf1);

			/* Skip the = */
			*sz1++ = '\0';
		}

		/* Locate the size. */
		if ((sz0 = strchr(buf0, '=')) == NULL)
			MALFORMED(buf0);

		/* Skip the = */
		*sz0++ = '\0';

		GETVAL(sz0);
		ASSIGN(buf0);

		if (buf1 != NULL) {
			GETVAL(sz1);
			ASSIGN(buf1);
		}
	}

#undef ASSIGN
#undef GETVAL
#undef MALFORMED

	if (strcmp(sep->se_proto, "unix") == 0) {
		sep->se_family = AF_LOCAL;
	} else {
		val = (int)strlen(sep->se_proto);
		if (!val) {
			syslog(LOG_ERR, "%s: invalid protocol specified",
			    sep->se_service);
			goto more;
		}
		val = sep->se_proto[val - 1];
		switch (val) {
		case '4':	/*tcp4 or udp4*/
			sep->se_family = AF_INET;
			break;
#ifdef INET6
		case '6':	/*tcp6 or udp6*/
			sep->se_family = AF_INET6;
			break;
#endif
		default:
			sep->se_family = AF_INET;	/*will become AF_INET6*/
			break;
		}
		if (strncmp(sep->se_proto, "rpc/", 4) == 0) {
#ifdef RPC
			char *cp1, *ccp;
			cp1 = strchr(sep->se_service, '/');
			if (cp1 == 0) {
				syslog(LOG_ERR, "%s: no rpc version",
				    sep->se_service);
				goto more;
			}
			*cp1++ = '\0';
			sep->se_rpcversl = sep->se_rpcversh =
			    (int)strtol(cp1, &ccp, 0);
			if (ccp == cp1) {
		badafterall:
				syslog(LOG_ERR, "%s/%s: bad rpc version",
				    sep->se_service, cp1);
				goto more;
			}
			if (*ccp == '-') {
				cp1 = ccp + 1;
				sep->se_rpcversh = (int)strtol(cp1, &ccp, 0);
				if (ccp == cp1)
					goto badafterall;
			}
#else
			syslog(LOG_ERR, "%s: rpc services not suported",
			    sep->se_service);
			goto more;
#endif /* RPC */
		}
	}
	arg = sskip(&cp);
	{
		char *cp1;
		if ((cp1 = strchr(arg, ':')) == NULL)
			cp1 = strchr(arg, '.');
		if (cp1 != NULL) {
			*cp1++ = '\0';
			sep->se_max = atoi(cp1);
		} else
			sep->se_max = TOOMANY;
	}
	sep->se_wait = strcmp(arg, "wait") == 0;
	if (ISMUX(sep)) {
		/*
		 * Silently enforce "nowait" for TCPMUX services since
		 * they don't have an assigned port to listen on.
		 */
		sep->se_wait = 0;

		if (strncmp(sep->se_proto, "tcp", 3)) {
			syslog(LOG_ERR, 
			    "%s: bad protocol for tcpmux service %s",
			    CONFIG, sep->se_service);
			goto more;
		}
		if (sep->se_socktype != SOCK_STREAM) {
			syslog(LOG_ERR, 
			    "%s: bad socket type for tcpmux service %s",
			    CONFIG, sep->se_service);
			goto more;
		}
	}
	sep->se_user = newstr(sskip(&cp));
	if ((sep->se_group = strchr(sep->se_user, ':')) != NULL)
		*sep->se_group++ = '\0';
	else if ((sep->se_group = strchr(sep->se_user, '.')) != NULL)
		*sep->se_group++ = '\0';

	sep->se_server = newstr(sskip(&cp));
	if (strcmp(sep->se_server, "internal") == 0) {
		struct biltin *bi;

		for (bi = biltins; bi->bi_service; bi++)
			if (bi->bi_socktype == sep->se_socktype &&
			    strcmp(bi->bi_service, sep->se_service) == 0)
				break;
		if (bi->bi_service == 0) {
			syslog(LOG_ERR, "internal service %s unknown",
			    sep->se_service);
			goto more;
		}
		sep->se_bi = bi;
		sep->se_wait = bi->bi_wait;
	} else
		sep->se_bi = NULL;
	argc = 0;
	for (arg = skip(&cp); cp; arg = skip(&cp)) {
		if (argc < MAXARGV)
			sep->se_argv[argc++] = newstr(arg);
	}
	while (argc <= MAXARGV)
		sep->se_argv[argc++] = NULL;
#ifdef IPSEC
	sep->se_policy = policy ? newstr(policy) : NULL;
#endif
	return (sep);
}

static void
freeconfig(struct servtab *cp)
{
	int i;

	if (cp->se_hostaddr)
		free(cp->se_hostaddr);
	if (cp->se_service)
		free(cp->se_service);
	if (cp->se_proto)
		free(cp->se_proto);
	if (cp->se_user)
		free(cp->se_user);
	/* Note: se_group is part of the newstr'ed se_user */
	if (cp->se_server)
		free(cp->se_server);
	for (i = 0; i < MAXARGV; i++)
		if (cp->se_argv[i])
			free(cp->se_argv[i]);
#ifdef IPSEC
	if (cp->se_policy)
		free(cp->se_policy);
#endif
}


/*
 * Safe skip - if skip returns null, log a syntax error in the
 * configuration file and exit.
 */
static char *
sskip(char **cpp)
{
	char *cp;

	cp = skip(cpp);
	if (cp == NULL) {
		syslog(LOG_ERR, "%s: syntax error", CONFIG);
		exit(1);
	}
	return (cp);
}

static char *
skip(char **cpp)
{
	char *cp = *cpp;
	char *start;
	char quote;

	if (*cpp == NULL)
		return (NULL);

again:
	while (*cp == ' ' || *cp == '\t')
		cp++;
	if (*cp == '\0') {
		int c;

		c = getc(fconfig);
		(void) ungetc(c, fconfig);
		if (c == ' ' || c == '\t')
			if ((cp = nextline(fconfig)) != NULL)
				goto again;
		*cpp = NULL;
		return (NULL);
	}
	start = cp;
	quote = '\0';
	while (*cp && (quote || (*cp != ' ' && *cp != '\t'))) {
		if (*cp == '\'' || *cp == '"') {
			if (quote && *cp != quote)
				cp++;
			else {
				if (quote)
					quote = '\0';
				else
					quote = *cp;
				memmove(cp, cp+1, strlen(cp));
			}
		} else
			cp++;
	}
	if (*cp != '\0')
		*cp++ = '\0';
	*cpp = cp;
	return (start);
}

static char *
nextline(FILE *fd)
{
	char *cp;

	if (fgets(line, (int)sizeof(line), fd) == NULL)
		return (NULL);
	cp = strchr(line, '\n');
	if (cp)
		*cp = '\0';
	return (line);
}

static char *
newstr(const char *cp)
{
	char *dp;
	if ((dp = strdup((cp != NULL) ? cp : "")) != NULL)
		return (dp);
	syslog(LOG_ERR, "strdup: %m");
	exit(1);
	/*NOTREACHED*/
}

static void
inetd_setproctitle(char *a, int s)
{
	socklen_t size;
	struct sockaddr_storage ss;
	char hbuf[NI_MAXHOST];
	const char *hp;
	struct sockaddr *sa;

	size = sizeof(ss);
	sa = (struct sockaddr *)(void *)&ss;
	if (getpeername(s, sa, &size) == 0) {
		if (getnameinfo(sa, size, hbuf, (socklen_t)sizeof(hbuf), NULL,
		    0, niflags) != 0)
			hp = "?";
		else
			hp = hbuf;
		setproctitle("-%s [%s]", a, hp);
	} else
		setproctitle("-%s", a);
}

static void
bump_nofile(void)
{
#define FD_CHUNK	32
	struct rlimit rl;

	if (getrlimit(RLIMIT_NOFILE, &rl) < 0) {
		syslog(LOG_ERR, "getrlimit: %m");
		return;
	}
	rl.rlim_cur = MIN(rl.rlim_max, rl.rlim_cur + FD_CHUNK);
	if (rl.rlim_cur <= rlim_ofile_cur) {
		syslog(LOG_ERR,
		    "bump_nofile: cannot extend file limit, max = %d",
		    (int)rl.rlim_cur);
		return;
	}

	if (setrlimit(RLIMIT_NOFILE, &rl) < 0) {
		syslog(LOG_ERR, "setrlimit: %m");
		return;
	}

	rlim_ofile_cur = rl.rlim_cur;
	return;
}

/*
 * Internet services provided internally by inetd:
 */
#define	BUFSIZE	4096

/* ARGSUSED */
static void
echo_stream(int s, struct servtab *sep)	/* Echo service -- echo data back */
{
	char buffer[BUFSIZE];
	ssize_t i;

	inetd_setproctitle(sep->se_service, s);
	while ((i = read(s, buffer, sizeof(buffer))) > 0 &&
	    write(s, buffer, (size_t)i) > 0)
		;
}

/* ARGSUSED */
static void
echo_dg(int s, struct servtab *sep)	/* Echo service -- echo data back */
{
	char buffer[BUFSIZE];
	ssize_t i;
	socklen_t size;
	struct sockaddr_storage ss;
	struct sockaddr *sa;

	sa = (struct sockaddr *)(void *)&ss;
	size = sizeof(ss);
	if ((i = recvfrom(s, buffer, sizeof(buffer), 0, sa, &size)) < 0)
		return;
	if (port_good_dg(sa))
		(void) sendto(s, buffer, (size_t)i, 0, sa, size);
}

/* ARGSUSED */
static void
discard_stream(int s, struct servtab *sep) /* Discard service -- ignore data */
{
	char buffer[BUFSIZE];

	inetd_setproctitle(sep->se_service, s);
	while ((errno = 0, read(s, buffer, sizeof(buffer)) > 0) ||
			errno == EINTR)
		;
}

/* ARGSUSED */
static void
discard_dg(int s, struct servtab *sep)	/* Discard service -- ignore data */
	
{
	char buffer[BUFSIZE];

	(void) read(s, buffer, sizeof(buffer));
}

#define LINESIZ 72
char ring[128];
char *endring;

static void
initring(void)
{
	int i;

	endring = ring;

	for (i = 0; i <= 128; ++i)
		if (isprint(i))
			*endring++ = i;
}

/* ARGSUSED */
static void
chargen_stream(int s,struct servtab *sep)	/* Character generator */
{
	size_t len;
	char *rs, text[LINESIZ+2];

	inetd_setproctitle(sep->se_service, s);

	if (!endring) {
		initring();
		rs = ring;
	}

	text[LINESIZ] = '\r';
	text[LINESIZ + 1] = '\n';
	for (rs = ring;;) {
		if ((len = endring - rs) >= LINESIZ)
			memmove(text, rs, LINESIZ);
		else {
			memmove(text, rs, len);
			memmove(text + len, ring, LINESIZ - len);
		}
		if (++rs == endring)
			rs = ring;
		if (write(s, text, sizeof(text)) != sizeof(text))
			break;
	}
}

/* ARGSUSED */
static void
chargen_dg(int s, struct servtab *sep)		/* Character generator */
{
	struct sockaddr_storage ss;
	struct sockaddr *sa;
	static char *rs;
	size_t len;
	socklen_t size;
	char text[LINESIZ+2];

	if (endring == 0) {
		initring();
		rs = ring;
	}

	sa = (struct sockaddr *)(void *)&ss;
	size = sizeof(ss);
	if (recvfrom(s, text, sizeof(text), 0, sa, &size) < 0)
		return;

	if (!port_good_dg(sa))
		return;

	if ((len = endring - rs) >= LINESIZ)
		memmove(text, rs, LINESIZ);
	else {
		memmove(text, rs, len);
		memmove(text + len, ring, LINESIZ - len);
	}
	if (++rs == endring)
		rs = ring;
	text[LINESIZ] = '\r';
	text[LINESIZ + 1] = '\n';
	(void) sendto(s, text, sizeof(text), 0, sa, size);
}

/*
 * Return a machine readable date and time, in the form of the
 * number of seconds since midnight, Jan 1, 1900.  Since gettimeofday
 * returns the number of seconds since midnight, Jan 1, 1970,
 * we must add 2208988800 seconds to this figure to make up for
 * some seventy years Bell Labs was asleep.
 */

static uint32_t
machtime(void)
{
	struct timeval tv;

	if (gettimeofday(&tv, NULL) < 0) {
		if (debug)
			fprintf(stderr, "Unable to get time of day\n");
		return (0);
	}
#define	OFFSET ((uint32_t)25567 * 24*60*60)
	return (htonl((uint32_t)(tv.tv_sec + OFFSET)));
#undef OFFSET
}

/* ARGSUSED */
static void
machtime_stream(int s, struct servtab *sep)
{
	uint32_t result;

	result = machtime();
	(void) write(s, &result, sizeof(result));
}

/* ARGSUSED */
void
machtime_dg(int s, struct servtab *sep)
{
	uint32_t result;
	struct sockaddr_storage ss;
	struct sockaddr *sa;
	socklen_t size;

	sa = (struct sockaddr *)(void *)&ss;
	size = sizeof(ss);
	if (recvfrom(s, &result, sizeof(result), 0, sa, &size) < 0)
		return;
	if (!port_good_dg(sa))
		return;
	result = machtime();
	(void)sendto(s, &result, sizeof(result), 0, sa, size);
}

/* ARGSUSED */
static void
daytime_stream(int s,struct servtab *sep)
/* Return human-readable time of day */
{
	char buffer[256];
	time_t clk;
	int len;

	clk = time((time_t *) 0);

	len = snprintf(buffer, sizeof buffer, "%.24s\r\n", ctime(&clk));
	(void) write(s, buffer, len);
}

/* ARGSUSED */
void
daytime_dg(int s, struct servtab *sep)
/* Return human-readable time of day */
{
	char buffer[256];
	time_t clk;
	struct sockaddr_storage ss;
	struct sockaddr *sa;
	socklen_t size;
	int len;

	clk = time((time_t *) 0);

	sa = (struct sockaddr *)(void *)&ss;
	size = sizeof(ss);
	if (recvfrom(s, buffer, sizeof(buffer), 0, sa, &size) < 0)
		return;
	if (!port_good_dg(sa))
		return;
	len = snprintf(buffer, sizeof buffer, "%.24s\r\n", ctime(&clk));
	(void) sendto(s, buffer, len, 0, sa, size);
}

/*
 * print_service:
 *	Dump relevant information to stderr
 */
static void
print_service(const char *action, struct servtab *sep)
{

	if (isrpcservice(sep))
		fprintf(stderr,
		    "%s: %s rpcprog=%d, rpcvers = %d/%d, proto=%s, wait.max=%d.%d, user:group=%s:%s builtin=%lx server=%s"
#ifdef IPSEC
		    " policy=\"%s\""
#endif
		    "\n",
		    action, sep->se_service,
		    sep->se_rpcprog, sep->se_rpcversh, sep->se_rpcversl, sep->se_proto,
		    sep->se_wait, sep->se_max, sep->se_user, sep->se_group,
		    (long)sep->se_bi, sep->se_server
#ifdef IPSEC
		    , (sep->se_policy ? sep->se_policy : "")
#endif
		    );
	else
		fprintf(stderr,
		    "%s: %s proto=%s%s, wait.max=%d.%d, user:group=%s:%s builtin=%lx server=%s"
#ifdef IPSEC
		    " policy=%s"
#endif
		    "\n",
		    action, sep->se_service,
		    sep->se_type == FAITH_TYPE ? "faith/" : "",
		    sep->se_proto,
		    sep->se_wait, sep->se_max, sep->se_user, sep->se_group,
		    (long)sep->se_bi, sep->se_server
#ifdef IPSEC
		    , (sep->se_policy ? sep->se_policy : "")
#endif
		    );
}

static void
usage(void)
{
#ifdef LIBWRAP
	(void)fprintf(stderr, "usage: %s [-dl] [conf]\n", getprogname());
#else
	(void)fprintf(stderr, "usage: %s [-d] [conf]\n", getprogname());
#endif
	exit(1);
}


/*
 *  Based on TCPMUX.C by Mark K. Lottor November 1988
 *  sri-nic::ps:<mkl>tcpmux.c
 */

static int		/* # of characters upto \r,\n or \0 */
get_line(int fd,	char *buf, int len)
{
	int count = 0;
	ssize_t n;

	do {
		n = read(fd, buf, len-count);
		if (n == 0)
			return (count);
		if (n < 0)
			return (-1);
		while (--n >= 0) {
			if (*buf == '\r' || *buf == '\n' || *buf == '\0')
				return (count);
			count++;
			buf++;
		}
	} while (count < len);
	return (count);
}

#define MAX_SERV_LEN	(256+2)		/* 2 bytes for \r\n */

#define strwrite(fd, buf)	(void) write(fd, buf, sizeof(buf)-1)

static void
tcpmux(int ctrl, struct servtab *sep)
{
	char service[MAX_SERV_LEN+1];
	int len;

	/* Get requested service name */
	if ((len = get_line(ctrl, service, MAX_SERV_LEN)) < 0) {
		strwrite(ctrl, "-Error reading service name\r\n");
		goto reject;
	}
	service[len] = '\0';

	if (debug)
		fprintf(stderr, "tcpmux: someone wants %s\n", service);

	/*
	 * Help is a required command, and lists available services,
	 * one per line.
	 */
	if (!strcasecmp(service, "help")) {
		strwrite(ctrl, "+Available services:\r\n");
		strwrite(ctrl, "help\r\n");
		for (sep = servtab; sep != NULL; sep = sep->se_next) {
			if (!ISMUX(sep))
				continue;
			(void)write(ctrl, sep->se_service,
			    strlen(sep->se_service));
			strwrite(ctrl, "\r\n");
		}
		goto reject;
	}

	/* Try matching a service in inetd.conf with the request */
	for (sep = servtab; sep != NULL; sep = sep->se_next) {
		if (!ISMUX(sep))
			continue;
		if (!strcasecmp(service, sep->se_service)) {
			if (ISMUXPLUS(sep))
				strwrite(ctrl, "+Go\r\n");
			run_service(ctrl, sep, 1 /* forked */);
			return;
		}
	}
	strwrite(ctrl, "-Service not available\r\n");
reject:
	_exit(1);
}

/*
 * check if the address/port where send data to is one of the obvious ports
 * that are used for denial of service attacks like two echo ports
 * just echoing data between them
 */
static int
port_good_dg(struct sockaddr *sa)
{
	struct in_addr in;
	struct sockaddr_in *sin;
#ifdef INET6
	struct in6_addr *in6;
	struct sockaddr_in6 *sin6;
#endif
	u_int16_t port;
	int i;
	char hbuf[NI_MAXHOST];

	switch (sa->sa_family) {
	case AF_INET:
		sin = (struct sockaddr_in *)(void *)sa;
		in.s_addr = ntohl(sin->sin_addr.s_addr);
		port = ntohs(sin->sin_port);
#ifdef INET6
	v4chk:
#endif
		if (IN_MULTICAST(in.s_addr))
			goto bad;
		switch ((in.s_addr & 0xff000000) >> 24) {
		case 0: case 127: case 255:
			goto bad;
		}
		if (dg_broadcast(&in))
			goto bad;
		break;
#ifdef INET6
	case AF_INET6:
		sin6 = (struct sockaddr_in6 *)(void *)sa;
		in6 = &sin6->sin6_addr;
		port = ntohs(sin6->sin6_port);
		if (IN6_IS_ADDR_MULTICAST(in6) || IN6_IS_ADDR_UNSPECIFIED(in6))
			goto bad;
		if (IN6_IS_ADDR_V4MAPPED(in6) || IN6_IS_ADDR_V4COMPAT(in6)) {
			memcpy(&in, &in6->s6_addr[12], sizeof(in));
			in.s_addr = ntohl(in.s_addr);
			goto v4chk;
		}
		break;
#endif
	default:
		/* XXX unsupported af, is it safe to assume it to be safe? */
		return (1);
	}

	for (i = 0; bad_ports[i] != 0; i++) {
		if (port == bad_ports[i])
			goto bad;
	}

	return (1);

bad:
	if (getnameinfo(sa, sa->sa_len, hbuf, (socklen_t)sizeof(hbuf), NULL, 0,
	    niflags) != 0)
		strlcpy(hbuf, "?", sizeof(hbuf));
	syslog(LOG_WARNING,"Possible DoS attack from %s, Port %d",
		hbuf, port);
	return (0);
}

/* XXX need optimization */
static int
dg_broadcast(struct in_addr *in)
{
	struct ifaddrs *ifa, *ifap;
	struct sockaddr_in *sin;

	if (getifaddrs(&ifap) < 0)
		return (0);
	for (ifa = ifap; ifa; ifa = ifa->ifa_next) {
		if (ifa->ifa_addr->sa_family != AF_INET ||
		    (ifa->ifa_flags & IFF_BROADCAST) == 0)
			continue;
		sin = (struct sockaddr_in *)(void *)ifa->ifa_broadaddr;
		if (sin->sin_addr.s_addr == in->s_addr) {
			freeifaddrs(ifap);
			return (1);
		}
	}
	freeifaddrs(ifap);
	return (0);
}

#ifndef __minix
static int
my_kevent(const struct kevent *changelist, size_t nchanges,
    struct kevent *eventlist, size_t nevents)
{
	int	result;

	while ((result = kevent(kq, changelist, nchanges, eventlist, nevents,
	    NULL)) < 0)
		if (errno != EINTR) {
			syslog(LOG_ERR, "kevent: %m");
			exit(EXIT_FAILURE);
		}

	return (result);
}

static struct kevent *
allocchange(void)
{
	if (changes == A_CNT(changebuf)) {
		(void) my_kevent(changebuf, A_CNT(changebuf), NULL, 0);
		changes = 0;
	}

	return (&changebuf[changes++]);
}
#endif /* !__minix */
