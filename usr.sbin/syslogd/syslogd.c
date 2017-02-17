/*	$NetBSD: syslogd.c,v 1.122 2015/09/05 20:19:43 dholland Exp $	*/

/*
 * Copyright (c) 1983, 1988, 1993, 1994
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
__COPYRIGHT("@(#) Copyright (c) 1983, 1988, 1993, 1994\
	The Regents of the University of California.  All rights reserved.");
#endif /* not lint */

#ifndef lint
#if 0
static char sccsid[] = "@(#)syslogd.c	8.3 (Berkeley) 4/4/94";
#else
__RCSID("$NetBSD: syslogd.c,v 1.122 2015/09/05 20:19:43 dholland Exp $");
#endif
#endif /* not lint */

/*
 *  syslogd -- log system messages
 *
 * This program implements a system log. It takes a series of lines.
 * Each line may have a priority, signified as "<n>" as
 * the first characters of the line.  If this is
 * not present, a default priority is used.
 *
 * To kill syslogd, send a signal 15 (terminate).  A signal 1 (hup) will
 * cause it to reread its configuration file.
 *
 * Defined Constants:
 *
 * MAXLINE -- the maximimum line length that can be handled.
 * DEFUPRI -- the default priority for user messages
 * DEFSPRI -- the default priority for kernel messages
 *
 * Author: Eric Allman
 * extensive changes by Ralph Campbell
 * more extensive changes by Eric Allman (again)
 * Extension to log by program name as well as facility and priority
 *   by Peter da Silva.
 * -U and -v by Harlan Stenn.
 * Priority comparison code by Harlan Stenn.
 * TLS, syslog-protocol, and syslog-sign code by Martin Schuette.
 */
#define SYSLOG_NAMES
#include <sys/stat.h>
#include <poll.h>
#include "syslogd.h"
#include "extern.h"

#ifndef DISABLE_SIGN
#include "sign.h"
struct sign_global_t GlobalSign = {
	.rsid = 0,
	.sig2_delims = STAILQ_HEAD_INITIALIZER(GlobalSign.sig2_delims)
};
#endif /* !DISABLE_SIGN */

#ifndef DISABLE_TLS
#include "tls.h"
#endif /* !DISABLE_TLS */

#ifdef LIBWRAP
int allow_severity = LOG_AUTH|LOG_INFO;
int deny_severity = LOG_AUTH|LOG_WARNING;
#endif

const char	*ConfFile = _PATH_LOGCONF;
char	ctty[] = _PATH_CONSOLE;

/*
 * Queue of about-to-be-dead processes we should watch out for.
 */
TAILQ_HEAD(, deadq_entry) deadq_head = TAILQ_HEAD_INITIALIZER(deadq_head);

typedef struct deadq_entry {
	pid_t				dq_pid;
	int				dq_timeout;
	TAILQ_ENTRY(deadq_entry)	dq_entries;
} *dq_t;

/*
 * The timeout to apply to processes waiting on the dead queue.	 Unit
 * of measure is "mark intervals", i.e. 20 minutes by default.
 * Processes on the dead queue will be terminated after that time.
 */
#define DQ_TIMO_INIT	2

/*
 * Intervals at which we flush out "message repeated" messages,
 * in seconds after previous message is logged.	 After each flush,
 * we move to the next interval until we reach the largest.
 */
int	repeatinterval[] = { 30, 120, 600 };	/* # of secs before flush */
#define MAXREPEAT ((sizeof(repeatinterval) / sizeof(repeatinterval[0])) - 1)
#define REPEATTIME(f)	((f)->f_time + repeatinterval[(f)->f_repeatcount])
#define BACKOFF(f)	{ if ((size_t)(++(f)->f_repeatcount) > MAXREPEAT) \
				 (f)->f_repeatcount = MAXREPEAT; \
			}

/* values for f_type */
#define F_UNUSED	0		/* unused entry */
#define F_FILE		1		/* regular file */
#define F_TTY		2		/* terminal */
#define F_CONSOLE	3		/* console terminal */
#define F_FORW		4		/* remote machine */
#define F_USERS		5		/* list of users */
#define F_WALL		6		/* everyone logged on */
#define F_PIPE		7		/* pipe to program */
#define F_FIFO		8		/* mkfifo(2) file */
#define F_TLS		9

struct TypeInfo {
	const char *name;
	char	   *queue_length_string;
	const char *default_length_string;
	char	   *queue_size_string;
	const char *default_size_string;
	int64_t	    queue_length;
	int64_t	    queue_size;
	int   max_msg_length;
} TypeInfo[] = {
	/* numeric values are set in init()
	 * -1 in length/size or max_msg_length means infinite */
	{"UNUSED",  NULL,    "0", NULL,	  "0", 0, 0,	 0},
	{"FILE",    NULL, "1024", NULL,	 "1M", 0, 0, 16384},
	{"TTY",	    NULL,    "0", NULL,	  "0", 0, 0,  1024},
	{"CONSOLE", NULL,    "0", NULL,	  "0", 0, 0,  1024},
	{"FORW",    NULL,    "0", NULL,	 "1M", 0, 0, 16384},
	{"USERS",   NULL,    "0", NULL,	  "0", 0, 0,  1024},
	{"WALL",    NULL,    "0", NULL,	  "0", 0, 0,  1024},
	{"PIPE",    NULL, "1024", NULL,	 "1M", 0, 0, 16384},
	{"FIFO",    NULL, "1024", NULL,	 "1M", 0, 0, 16384},
#ifndef DISABLE_TLS
	{"TLS",	    NULL,   "-1", NULL, "16M", 0, 0, 16384}
#endif /* !DISABLE_TLS */
};

struct	filed *Files = NULL;
struct	filed consfile;

time_t	now;
int	Debug = D_NONE;		/* debug flag */
int	daemonized = 0;		/* we are not daemonized yet */
char	*LocalFQDN = NULL;	       /* our FQDN */
char	*oldLocalFQDN = NULL;	       /* our previous FQDN */
char	LocalHostName[MAXHOSTNAMELEN]; /* our hostname */
struct socketEvent *finet;	/* Internet datagram sockets and events */
int   *funix;			/* Unix domain datagram sockets */
#ifndef DISABLE_TLS
struct socketEvent *TLS_Listen_Set; /* TLS/TCP sockets and events */
#endif /* !DISABLE_TLS */
int	Initialized = 0;	/* set when we have initialized ourselves */
int	ShuttingDown;		/* set when we die() */
int	MarkInterval = 20 * 60; /* interval between marks in seconds */
int	MarkSeq = 0;		/* mark sequence number */
int	SecureMode = 0;		/* listen only on unix domain socks */
int	UseNameService = 1;	/* make domain name queries */
int	NumForwards = 0;	/* number of forwarding actions in conf file */
char	**LogPaths;		/* array of pathnames to read messages from */
int	NoRepeat = 0;		/* disable "repeated"; log always */
int	RemoteAddDate = 0;	/* always add date to messages from network */
int	SyncKernel = 0;		/* write kernel messages synchronously */
int	UniquePriority = 0;	/* only log specified priority */
int	LogFacPri = 0;		/* put facility and priority in log messages: */
				/* 0=no, 1=numeric, 2=names */
bool	BSDOutputFormat = true;	/* if true emit traditional BSD Syslog lines,
				 * otherwise new syslog-protocol lines
				 *
				 * Open Issue: having a global flag is the
				 * easiest solution. If we get a more detailed
				 * config file this could/should be changed
				 * into a destination-specific flag.
				 * Most output code should be ready to handle
				 * this, it will only break some syslog-sign
				 * configurations (e.g. with SG="0").
				 */
char	appname[]   = "syslogd";/* the APPNAME for own messages */
char   *include_pid = NULL;	/* include PID in own messages */


/* init and setup */
void		usage(void) __attribute__((__noreturn__));
void		logpath_add(char ***, int *, int *, const char *);
void		logpath_fileadd(char ***, int *, int *, const char *);
void		init(int fd, short event, void *ev);  /* SIGHUP kevent dispatch routine */
struct socketEvent*
		socksetup(int, const char *);
int		getmsgbufsize(void);
char	       *getLocalFQDN(void);
void		trim_anydomain(char *);
/* pipe & subprocess handling */
int		p_open(char *, pid_t *);
void		deadq_enter(pid_t, const char *);
int		deadq_remove(pid_t);
void		log_deadchild(pid_t, int, const char *);
void		reapchild(int fd, short event, void *ev); /* SIGCHLD kevent dispatch routine */
/* input message parsing & formatting */
const char     *cvthname(struct sockaddr_storage *);
void		printsys(char *);
struct buf_msg *printline_syslogprotocol(const char*, char*, int, int);
struct buf_msg *printline_bsdsyslog(const char*, char*, int, int);
struct buf_msg *printline_kernelprintf(const char*, char*, int, int);
size_t		check_timestamp(unsigned char *, char **, bool, bool);
char	       *copy_utf8_ascii(char*, size_t);
uint_fast32_t	get_utf8_value(const char*);
unsigned	valid_utf8(const char *);
static unsigned check_sd(char*);
static unsigned check_msgid(char *);
/* event handling */
static void	dispatch_read_klog(int fd, short event, void *ev);
static void	dispatch_read_finet(int fd, short event, void *ev);
static void	dispatch_read_funix(int fd, short event, void *ev);
static void	domark(int fd, short event, void *ev); /* timer kevent dispatch routine */
/* log messages */
void		logmsg_async(int, const char *, const char *, int);
void		logmsg(struct buf_msg *);
int		matches_spec(const char *, const char *,
		char *(*)(const char *, const char *));
void		udp_send(struct filed *, char *, size_t);
void		wallmsg(struct filed *, struct iovec *, size_t);
/* buffer & queue functions */
size_t		message_queue_purge(struct filed *f, size_t, int);
size_t		message_allqueues_check(void);
static struct buf_queue *
		find_qentry_to_delete(const struct buf_queue_head *, int, bool);
struct buf_queue *
		message_queue_add(struct filed *, struct buf_msg *);
size_t		buf_queue_obj_size(struct buf_queue*);
/* configuration & parsing */
void		cfline(size_t, const char *, struct filed *, const char *,
    const char *);
void		read_config_file(FILE*, struct filed**);
void		store_sign_delim_sg2(char*);
int		decode(const char *, CODE *);
bool		copy_config_value(const char *, char **, const char **,
    const char *, int);
bool		copy_config_value_word(char **, const char **);

/* config parsing */
#ifndef DISABLE_TLS
void		free_cred_SLIST(struct peer_cred_head *);
static inline void
		free_incoming_tls_sockets(void);
#endif /* !DISABLE_TLS */
static int writev1(int, struct iovec *, size_t);

/* for make_timestamp() */
char	timestamp[MAX_TIMESTAMPLEN + 1];
/*
 * Global line buffer.	Since we only process one event at a time,
 * a global one will do.  But for klog, we use own buffer so that
 * partial line at the end of buffer can be deferred.
 */
char *linebuf, *klog_linebuf;
size_t linebufsize, klog_linebufoff;

static const char *bindhostname = NULL;

#ifndef DISABLE_TLS
struct TLS_Incoming TLS_Incoming_Head = \
	SLIST_HEAD_INITIALIZER(TLS_Incoming_Head);
extern char *SSL_ERRCODE[];
struct tls_global_options_t tls_opt;
#endif /* !DISABLE_TLS */

int
main(int argc, char *argv[])
{
	int ch, j, fklog;
	int funixsize = 0, funixmaxsize = 0;
	struct sockaddr_un sunx;
	char **pp;
	struct event *ev;
	uid_t uid = 0;
	gid_t gid = 0;
	char *user = NULL;
	char *group = NULL;
	const char *root = "/";
	char *endp;
	struct group   *gr;
	struct passwd  *pw;
	unsigned long l;

	/* should we set LC_TIME="C" to ensure correct timestamps&parsing? */
	(void)setlocale(LC_ALL, "");

	while ((ch = getopt(argc, argv, "b:dnsSf:m:o:p:P:ru:g:t:TUv")) != -1)
		switch(ch) {
		case 'b':
			bindhostname = optarg;
			break;
		case 'd':		/* debug */
			Debug = D_DEFAULT;
			/* is there a way to read the integer value
			 * for Debug as an optional argument? */
			break;
		case 'f':		/* configuration file */
			ConfFile = optarg;
			break;
		case 'g':
			group = optarg;
			if (*group == '\0')
				usage();
			break;
		case 'm':		/* mark interval */
			MarkInterval = atoi(optarg) * 60;
			break;
		case 'n':		/* turn off DNS queries */
			UseNameService = 0;
			break;
		case 'o':		/* message format */
#define EQ(a)		(strncmp(optarg, # a, sizeof(# a) - 1) == 0)
			if (EQ(bsd) || EQ(rfc3264))
				BSDOutputFormat = true;
			else if (EQ(syslog) || EQ(rfc5424))
				BSDOutputFormat = false;
			else
				usage();
			/* TODO: implement additional output option "osyslog"
			 *	 for old syslogd behaviour as introduced after
			 *	 FreeBSD PR#bin/7055.
			 */
			break;
		case 'p':		/* path */
			logpath_add(&LogPaths, &funixsize,
			    &funixmaxsize, optarg);
			break;
		case 'P':		/* file of paths */
			logpath_fileadd(&LogPaths, &funixsize,
			    &funixmaxsize, optarg);
			break;
		case 'r':		/* disable "repeated" compression */
			NoRepeat++;
			break;
		case 's':		/* no network listen mode */
			SecureMode++;
			break;
		case 'S':
			SyncKernel = 1;
			break;
		case 't':
			root = optarg;
			if (*root == '\0')
				usage();
			break;
		case 'T':
			RemoteAddDate = 1;
			break;
		case 'u':
			user = optarg;
			if (*user == '\0')
				usage();
			break;
		case 'U':		/* only log specified priority */
			UniquePriority = 1;
			break;
		case 'v':		/* log facility and priority */
			if (LogFacPri < 2)
				LogFacPri++;
			break;
		default:
			usage();
		}
	if ((argc -= optind) != 0)
		usage();

	setlinebuf(stdout);
	tzset(); /* init TZ information for localtime. */

	if (user != NULL) {
		if (isdigit((unsigned char)*user)) {
			errno = 0;
			endp = NULL;
			l = strtoul(user, &endp, 0);
			if (errno || *endp != '\0')
				goto getuser;
			uid = (uid_t)l;
			if (uid != l) {/* TODO: never executed */
				errno = 0;
				logerror("UID out of range");
				die(0, 0, NULL);
			}
		} else {
getuser:
			if ((pw = getpwnam(user)) != NULL) {
				uid = pw->pw_uid;
			} else {
				errno = 0;
				logerror("Cannot find user `%s'", user);
				die(0, 0, NULL);
			}
		}
	}

	if (group != NULL) {
		if (isdigit((unsigned char)*group)) {
			errno = 0;
			endp = NULL;
			l = strtoul(group, &endp, 0);
			if (errno || *endp != '\0')
				goto getgroup;
			gid = (gid_t)l;
			if (gid != l) {/* TODO: never executed */
				errno = 0;
				logerror("GID out of range");
				die(0, 0, NULL);
			}
		} else {
getgroup:
			if ((gr = getgrnam(group)) != NULL) {
				gid = gr->gr_gid;
			} else {
				errno = 0;
				logerror("Cannot find group `%s'", group);
				die(0, 0, NULL);
			}
		}
	}

	if (access(root, F_OK | R_OK)) {
		logerror("Cannot access `%s'", root);
		die(0, 0, NULL);
	}

	consfile.f_type = F_CONSOLE;
	(void)strlcpy(consfile.f_un.f_fname, ctty,
	    sizeof(consfile.f_un.f_fname));
	linebufsize = getmsgbufsize();
	if (linebufsize < MAXLINE)
		linebufsize = MAXLINE;
	linebufsize++;

	if (!(linebuf = malloc(linebufsize))) {
		logerror("Couldn't allocate buffer");
		die(0, 0, NULL);
	}
	if (!(klog_linebuf = malloc(linebufsize))) {
		logerror("Couldn't allocate buffer for klog");
		die(0, 0, NULL);
	}


#ifndef SUN_LEN
#define SUN_LEN(unp) (strlen((unp)->sun_path) + 2)
#endif
	if (funixsize == 0)
		logpath_add(&LogPaths, &funixsize,
		    &funixmaxsize, _PATH_LOG);
	funix = malloc(sizeof(*funix) * funixsize);
	if (funix == NULL) {
		logerror("Couldn't allocate funix descriptors");
		die(0, 0, NULL);
	}
	for (j = 0, pp = LogPaths; *pp; pp++, j++) {
		DPRINTF(D_NET, "Making unix dgram socket `%s'\n", *pp);
		unlink(*pp);
		memset(&sunx, 0, sizeof(sunx));
		sunx.sun_family = AF_LOCAL;
		(void)strncpy(sunx.sun_path, *pp, sizeof(sunx.sun_path));
		funix[j] = socket(AF_LOCAL, SOCK_DGRAM, 0);
		if (funix[j] < 0 || bind(funix[j],
		    (struct sockaddr *)&sunx, SUN_LEN(&sunx)) < 0 ||
		    chmod(*pp, 0666) < 0) {
			logerror("Cannot create `%s'", *pp);
			die(0, 0, NULL);
		}
		DPRINTF(D_NET, "Listening on unix dgram socket `%s'\n", *pp);
	}

	if ((fklog = open(_PATH_KLOG, O_RDONLY, 0)) < 0) {
		DPRINTF(D_FILE, "Can't open `%s' (%d)\n", _PATH_KLOG, errno);
	} else {
		DPRINTF(D_FILE, "Listening on kernel log `%s' with fd %d\n",
		    _PATH_KLOG, fklog);
	}

#if (!defined(DISABLE_TLS) && !defined(DISABLE_SIGN))
	/* basic OpenSSL init */
	SSL_load_error_strings();
	(void) SSL_library_init();
	OpenSSL_add_all_digests();
	/* OpenSSL PRNG needs /dev/urandom, thus initialize before chroot() */
	if (!RAND_status()) {
		errno = 0;
		logerror("Unable to initialize OpenSSL PRNG");
	} else {
		DPRINTF(D_TLS, "Initializing PRNG\n");
	}
#endif /* (!defined(DISABLE_TLS) && !defined(DISABLE_SIGN)) */
#ifndef DISABLE_SIGN
	/* initialize rsid -- we will use that later to determine
	 * whether sign_global_init() was already called */
	GlobalSign.rsid = 0;
#endif /* !DISABLE_SIGN */
#if (IETF_NUM_PRIVALUES != (LOG_NFACILITIES<<3))
	logerror("Warning: system defines %d priority values, but "
	    "syslog-protocol/syslog-sign specify %d values",
	    LOG_NFACILITIES, SIGN_NUM_PRIVALS);
#endif

	/*
	 * All files are open, we can drop privileges and chroot
	 */
	DPRINTF(D_MISC, "Attempt to chroot to `%s'\n", root);
	if (chroot(root) == -1) {
		logerror("Failed to chroot to `%s'", root);
		die(0, 0, NULL);
	}
	DPRINTF(D_MISC, "Attempt to set GID/EGID to `%d'\n", gid);
	if (setgid(gid) || setegid(gid)) {
		logerror("Failed to set gid to `%d'", gid);
		die(0, 0, NULL);
	}
	DPRINTF(D_MISC, "Attempt to set UID/EUID to `%d'\n", uid);
	if (setuid(uid) || seteuid(uid)) {
		logerror("Failed to set uid to `%d'", uid);
		die(0, 0, NULL);
	}
	/*
	 * We cannot detach from the terminal before we are sure we won't
	 * have a fatal error, because error message would not go to the
	 * terminal and would not be logged because syslogd dies.
	 * All die() calls are behind us, we can call daemon()
	 */
	if (!Debug) {
		(void)daemon(0, 0);
		daemonized = 1;
		/* tuck my process id away, if i'm not in debug mode */
#ifdef __NetBSD_Version__
		pidfile(NULL);
#endif /* __NetBSD_Version__ */
	}

#define MAX_PID_LEN 5
	include_pid = malloc(MAX_PID_LEN+1);
	snprintf(include_pid, MAX_PID_LEN+1, "%d", getpid());

	/*
	 * Create the global kernel event descriptor.
	 *
	 * NOTE: We MUST do this after daemon(), bacause the kqueue()
	 * API dictates that kqueue descriptors are not inherited
	 * across forks (lame!).
	 */
	(void)event_init();

	/*
	 * We must read the configuration file for the first time
	 * after the kqueue descriptor is created, because we install
	 * events during this process.
	 */
	init(0, 0, NULL);

	/*
	 * Always exit on SIGTERM.  Also exit on SIGINT and SIGQUIT
	 * if we're debugging.
	 */
	(void)signal(SIGTERM, SIG_IGN);
	(void)signal(SIGINT, SIG_IGN);
	(void)signal(SIGQUIT, SIG_IGN);

	ev = allocev();
	signal_set(ev, SIGTERM, die, ev);
	EVENT_ADD(ev);

	if (Debug) {
		ev = allocev();
		signal_set(ev, SIGINT, die, ev);
		EVENT_ADD(ev);
		ev = allocev();
		signal_set(ev, SIGQUIT, die, ev);
		EVENT_ADD(ev);
	}

	ev = allocev();
	signal_set(ev, SIGCHLD, reapchild, ev);
	EVENT_ADD(ev);

	ev = allocev();
	schedule_event(&ev,
		&((struct timeval){TIMERINTVL, 0}),
		domark, ev);

	(void)signal(SIGPIPE, SIG_IGN); /* We'll catch EPIPE instead. */

	/* Re-read configuration on SIGHUP. */
	(void) signal(SIGHUP, SIG_IGN);
	ev = allocev();
	signal_set(ev, SIGHUP, init, ev);
	EVENT_ADD(ev);

#ifndef DISABLE_TLS
	ev = allocev();
	signal_set(ev, SIGUSR1, dispatch_force_tls_reconnect, ev);
	EVENT_ADD(ev);
#endif /* !DISABLE_TLS */

	if (fklog >= 0) {
		ev = allocev();
		DPRINTF(D_EVENT,
			"register klog for fd %d with ev@%p\n", fklog, ev);
		event_set(ev, fklog, EV_READ | EV_PERSIST,
			dispatch_read_klog, ev);
		EVENT_ADD(ev);
	}
	for (j = 0, pp = LogPaths; *pp; pp++, j++) {
		ev = allocev();
		event_set(ev, funix[j], EV_READ | EV_PERSIST,
			dispatch_read_funix, ev);
		EVENT_ADD(ev);
	}

	DPRINTF(D_MISC, "Off & running....\n");

	j = event_dispatch();
	/* normal termination via die(), reaching this is an error */
	DPRINTF(D_MISC, "event_dispatch() returned %d\n", j);
	die(0, 0, NULL);
	/*NOTREACHED*/
	return 0;
}

void
usage(void)
{

	(void)fprintf(stderr,
	    "usage: %s [-dnrSsTUv] [-b bind_address] [-f config_file] [-g group]\n"
	    "\t[-m mark_interval] [-P file_list] [-p log_socket\n"
	    "\t[-p log_socket2 ...]] [-t chroot_dir] [-u user]\n",
	    getprogname());
	exit(1);
}

/*
 * Dispatch routine for reading /dev/klog
 *
 * Note: slightly different semantic in dispatch_read functions:
 *	 - read_klog() might give multiple messages in linebuf and
 *	   leaves the task of splitting them to printsys()
 *	 - all other read functions receive one message and
 *	   then call printline() with one buffer.
 */
static void
dispatch_read_klog(int fd, short event, void *ev)
{
	ssize_t rv;
	size_t resid = linebufsize - klog_linebufoff;

	DPRINTF((D_CALL|D_EVENT), "Kernel log active (%d, %d, %p)"
		" with linebuf@%p, length %zu)\n", fd, event, ev,
		klog_linebuf, linebufsize);

	rv = read(fd, &klog_linebuf[klog_linebufoff], resid - 1);
	if (rv > 0) {
		klog_linebuf[klog_linebufoff + rv] = '\0';
		printsys(klog_linebuf);
	} else if (rv < 0 && errno != EINTR) {
		/*
		 * /dev/klog has croaked.  Disable the event
		 * so it won't bother us again.
		 */
		logerror("klog failed");
		event_del(ev);
	}
}

/*
 * Dispatch routine for reading Unix domain sockets.
 */
static void
dispatch_read_funix(int fd, short event, void *ev)
{
	struct sockaddr_un myname, fromunix;
	ssize_t rv;
	socklen_t sunlen;

	sunlen = sizeof(myname);
	if (getsockname(fd, (struct sockaddr *)&myname, &sunlen) != 0) {
		/*
		 * This should never happen, so ensure that it doesn't
		 * happen again.
		 */
		logerror("getsockname() unix failed");
		event_del(ev);
		return;
	}

#define SUN_PATHLEN(su) \
	((su)->sun_len - (sizeof(*(su)) - sizeof((su)->sun_path)))

	DPRINTF((D_CALL|D_EVENT|D_NET), "Unix socket (%.*s) active (%d, %d %p)"
		" with linebuf@%p, size %zu)\n", (int)SUN_PATHLEN(&myname),
		myname.sun_path, fd, event, ev, linebuf, linebufsize-1);

	sunlen = sizeof(fromunix);
	rv = recvfrom(fd, linebuf, linebufsize-1, 0,
	    (struct sockaddr *)&fromunix, &sunlen);
	if (rv > 0) {
		linebuf[rv] = '\0';
		printline(LocalFQDN, linebuf, 0);
	} else if (rv < 0 && errno != EINTR) {
		logerror("recvfrom() unix `%.*s'",
			(int)SUN_PATHLEN(&myname), myname.sun_path);
	}
}

/*
 * Dispatch routine for reading Internet sockets.
 */
static void
dispatch_read_finet(int fd, short event, void *ev)
{
#ifdef LIBWRAP
	struct request_info req;
#endif
	struct sockaddr_storage frominet;
	ssize_t rv;
	socklen_t len;
	int reject = 0;

	DPRINTF((D_CALL|D_EVENT|D_NET), "inet socket active (%d, %d %p) "
		" with linebuf@%p, size %zu)\n",
		fd, event, ev, linebuf, linebufsize-1);

#ifdef LIBWRAP
	request_init(&req, RQ_DAEMON, appname, RQ_FILE, fd, NULL);
	fromhost(&req);
	reject = !hosts_access(&req);
	if (reject)
		DPRINTF(D_NET, "access denied\n");
#endif

	len = sizeof(frominet);
	rv = recvfrom(fd, linebuf, linebufsize-1, 0,
	    (struct sockaddr *)&frominet, &len);
	if (rv == 0 || (rv < 0 && errno == EINTR))
		return;
	else if (rv < 0) {
		logerror("recvfrom inet");
		return;
	}

	linebuf[rv] = '\0';
	if (!reject)
		printline(cvthname(&frominet), linebuf,
		    RemoteAddDate ? ADDDATE : 0);
}

/*
 * given a pointer to an array of char *'s, a pointer to its current
 * size and current allocated max size, and a new char * to add, add
 * it, update everything as necessary, possibly allocating a new array
 */
void
logpath_add(char ***lp, int *szp, int *maxszp, const char *new)
{
	char **nlp;
	int newmaxsz;

	DPRINTF(D_FILE, "Adding `%s' to the %p logpath list\n", new, *lp);
	if (*szp == *maxszp) {
		if (*maxszp == 0) {
			newmaxsz = 4;	/* start of with enough for now */
			*lp = NULL;
		} else
			newmaxsz = *maxszp * 2;
		nlp = realloc(*lp, sizeof(char *) * (newmaxsz + 1));
		if (nlp == NULL) {
			logerror("Couldn't allocate line buffer");
			die(0, 0, NULL);
		}
		*lp = nlp;
		*maxszp = newmaxsz;
	}
	if (((*lp)[(*szp)++] = strdup(new)) == NULL) {
		logerror("Couldn't allocate logpath");
		die(0, 0, NULL);
	}
	(*lp)[(*szp)] = NULL;		/* always keep it NULL terminated */
}

/* do a file of log sockets */
void
logpath_fileadd(char ***lp, int *szp, int *maxszp, const char *file)
{
	FILE *fp;
	char *line;
	size_t len;

	fp = fopen(file, "r");
	if (fp == NULL) {
		logerror("Could not open socket file list `%s'", file);
		die(0, 0, NULL);
	}

	while ((line = fgetln(fp, &len)) != NULL) {
		line[len - 1] = 0;
		logpath_add(lp, szp, maxszp, line);
	}
	fclose(fp);
}

/*
 * checks UTF-8 codepoint
 * returns either its length in bytes or 0 if *input is invalid
*/
unsigned
valid_utf8(const char *c) {
	unsigned rc, nb;

	/* first byte gives sequence length */
	     if ((*c & 0x80) == 0x00) return 1; /* 0bbbbbbb -- ASCII */
	else if ((*c & 0xc0) == 0x80) return 0; /* 10bbbbbb -- trailing byte */
	else if ((*c & 0xe0) == 0xc0) nb = 2;	/* 110bbbbb */
	else if ((*c & 0xf0) == 0xe0) nb = 3;	/* 1110bbbb */
	else if ((*c & 0xf8) == 0xf0) nb = 4;	/* 11110bbb */
	else return 0; /* UTF-8 allows only up to 4 bytes */

	/* catch overlong encodings */
	if ((*c & 0xfe) == 0xc0)
		return 0; /* 1100000b ... */
	else if (((*c & 0xff) == 0xe0) && ((*(c+1) & 0xe0) == 0x80))
		return 0; /* 11100000 100bbbbb ... */
	else if (((*c & 0xff) == 0xf0) && ((*(c+1) & 0xf0) == 0x80))
		return 0; /* 11110000 1000bbbb ... ... */

	/* and also filter UTF-16 surrogates (=invalid in UTF-8) */
	if (((*c & 0xff) == 0xed) && ((*(c+1) & 0xe0) == 0xa0))
		return 0; /* 11101101 101bbbbb ... */

	rc = nb;
	/* check trailing bytes */
	switch (nb) {
	default: return 0;
	case 4: if ((*(c+3) & 0xc0) != 0x80) return 0; /*FALLTHROUGH*/
	case 3: if ((*(c+2) & 0xc0) != 0x80) return 0; /*FALLTHROUGH*/
	case 2: if ((*(c+1) & 0xc0) != 0x80) return 0; /*FALLTHROUGH*/
	}
	return rc;
}
#define UTF8CHARMAX 4

/*
 * read UTF-8 value
 * returns a the codepoint number
 */
uint_fast32_t
get_utf8_value(const char *c) {
	uint_fast32_t sum;
	unsigned nb, i;

	/* first byte gives sequence length */
	     if ((*c & 0x80) == 0x00) return *c;/* 0bbbbbbb -- ASCII */
	else if ((*c & 0xc0) == 0x80) return 0; /* 10bbbbbb -- trailing byte */
	else if ((*c & 0xe0) == 0xc0) {		/* 110bbbbb */
		nb = 2;
		sum = (*c & ~0xe0) & 0xff;
	} else if ((*c & 0xf0) == 0xe0) {	/* 1110bbbb */
		nb = 3;
		sum = (*c & ~0xf0) & 0xff;
	} else if ((*c & 0xf8) == 0xf0) {	/* 11110bbb */
		nb = 4;
		sum = (*c & ~0xf8) & 0xff;
	} else return 0; /* UTF-8 allows only up to 4 bytes */

	/* check trailing bytes -- 10bbbbbb */
	i = 1;
	while (i < nb) {
		sum <<= 6;
		sum |= ((*(c+i) & ~0xc0) & 0xff);
		i++;
	}
	return sum;
}

/* note previous versions transscribe
 * control characters, e.g. \007 --> "^G"
 * did anyone rely on that?
 *
 * this new version works on only one buffer and
 * replaces control characters with a space
 */
#define NEXTFIELD(ptr) if (*(p) == ' ') (p)++; /* SP */			\
		       else {						\
				DPRINTF(D_DATA, "format error\n");	\
				if (*(p) == '\0') start = (p);		\
				goto all_syslog_msg;			\
		       }
#define FORCE2ASCII(c) ((iscntrl((unsigned char)(c)) && (c) != '\t')	\
			? ((c) == '\n' ? ' ' : '?')			\
			: (c) & 0177)

/* following syslog-protocol */
#define printusascii(ch) (ch >= 33 && ch <= 126)
#define sdname(ch) (ch != '=' && ch != ' ' \
		 && ch != ']' && ch != '"' \
		 && printusascii(ch))

/* checks whether the first word of string p can be interpreted as
 * a syslog-protocol MSGID and if so returns its length.
 *
 * otherwise returns 0
 */
static unsigned
check_msgid(char *p)
{
	char *q = p;

	/* consider the NILVALUE to be valid */
	if (*q == '-' && *(q+1) == ' ')
		return 1;

	for (;;) {
		if (*q == ' ')
			return q - p;
		else if (*q == '\0' || !printusascii(*q) || q - p >= MSGID_MAX)
			return 0;
		else
			q++;
	}
}

/*
 * returns number of chars found in SD at beginning of string p
 * thus returns 0 if no valid SD is found
 *
 * if ascii == true then substitute all non-ASCII chars
 * otherwise use syslog-protocol rules to allow UTF-8 in values
 * note: one pass for filtering and scanning, so a found SD
 * is always filtered, but an invalid one could be partially
 * filtered up to the format error.
 */
static unsigned
check_sd(char* p)
{
	char *q = p;
	bool esc = false;

	/* consider the NILVALUE to be valid */
	if (*q == '-' && (*(q+1) == ' ' || *(q+1) == '\0'))
		return 1;

	for(;;) { /* SD-ELEMENT */
		if (*q++ != '[') return 0;
		/* SD-ID */
		if (!sdname(*q)) return 0;
		while (sdname(*q)) {
			*q = FORCE2ASCII(*q);
			q++;
		}
		for(;;) { /* SD-PARAM */
			if (*q == ']') {
				q++;
				if (*q == ' ' || *q == '\0') return q - p;
				else if (*q == '[') break;
			} else if (*q++ != ' ') return 0;

			/* PARAM-NAME */
			if (!sdname(*q)) return 0;
			while (sdname(*q)) {
				*q = FORCE2ASCII(*q);
				q++;
			}

			if (*q++ != '=') return 0;
			if (*q++ != '"') return 0;

			for(;;) { /* PARAM-VALUE */
				if (esc) {
					esc = false;
					if (*q == '\\' || *q == '"' ||
					    *q == ']') {
						q++;
						continue;
					}
					/* no else because invalid
					 * escape sequences are accepted */
				}
				else if (*q == '"') break;
				else if (*q == '\0' || *q == ']') return 0;
				else if (*q == '\\') esc = true;
				else {
					int i;
					i = valid_utf8(q);
					if (i == 0)
						*q = '?';
					else if (i == 1)
						*q = FORCE2ASCII(*q);
					else /* multi byte char */
						q += (i-1);
				}
				q++;
			}
			q++;
		}
	}
}

struct buf_msg *
printline_syslogprotocol(const char *hname, char *msg,
	int flags, int pri)
{
	struct buf_msg *buffer;
	char *p, *start;
	unsigned sdlen = 0, i = 0;
	bool utf8allowed = false; /* for some fields */

	DPRINTF((D_CALL|D_BUFFER|D_DATA), "printline_syslogprotocol("
	    "\"%s\", \"%s\", %d, %d)\n", hname, msg, flags, pri);

	buffer = buf_msg_new(0);
	p = msg;
	p += check_timestamp((unsigned char*) p,
		&buffer->timestamp, true, !BSDOutputFormat);
	DPRINTF(D_DATA, "Got timestamp \"%s\"\n", buffer->timestamp);

	if (flags & ADDDATE) {
		FREEPTR(buffer->timestamp);
		buffer->timestamp = make_timestamp(NULL, !BSDOutputFormat, 0);
	}

	start = p;
	NEXTFIELD(p);
	/* extract host */
	for (start = p;; p++) {
		if ((*p == ' ' || *p == '\0')
		    && start == p-1 && *(p-1) == '-') {
			/* NILVALUE */
			break;
		} else if ((*p == ' ' || *p == '\0')
		    && (start != p-1 || *(p-1) != '-')) {
			buffer->host = strndup(start, p - start);
			break;
		} else {
			*p = FORCE2ASCII(*p);
		}
	}
	/* p @ SP after host */
	DPRINTF(D_DATA, "Got host \"%s\"\n", buffer->host);

	/* extract app-name */
	NEXTFIELD(p);
	for (start = p;; p++) {
		if ((*p == ' ' || *p == '\0')
		    && start == p-1 && *(p-1) == '-') {
			/* NILVALUE */
			break;
		} else if ((*p == ' ' || *p == '\0')
		    && (start != p-1 || *(p-1) != '-')) {
			buffer->prog = strndup(start, p - start);
			break;
		} else {
			*p = FORCE2ASCII(*p);
		}
	}
	DPRINTF(D_DATA, "Got prog \"%s\"\n", buffer->prog);

	/* extract procid */
	NEXTFIELD(p);
	for (start = p;; p++) {
		if ((*p == ' ' || *p == '\0')
		    && start == p-1 && *(p-1) == '-') {
			/* NILVALUE */
			break;
		} else if ((*p == ' ' || *p == '\0')
		    && (start != p-1 || *(p-1) != '-')) {
			buffer->pid = strndup(start, p - start);
			start = p;
			break;
		} else {
			*p = FORCE2ASCII(*p);
		}
	}
	DPRINTF(D_DATA, "Got pid \"%s\"\n", buffer->pid);

	/* extract msgid */
	NEXTFIELD(p);
	for (start = p;; p++) {
		if ((*p == ' ' || *p == '\0')
		    && start == p-1 && *(p-1) == '-') {
			/* NILVALUE */
			start = p+1;
			break;
		} else if ((*p == ' ' || *p == '\0')
		    && (start != p-1 || *(p-1) != '-')) {
			buffer->msgid = strndup(start, p - start);
			start = p+1;
			break;
		} else {
			*p = FORCE2ASCII(*p);
		}
	}
	DPRINTF(D_DATA, "Got msgid \"%s\"\n", buffer->msgid);

	/* extract SD */
	NEXTFIELD(p);
	start = p;
	sdlen = check_sd(p);
	DPRINTF(D_DATA, "check_sd(\"%s\") returned %d\n", p, sdlen);

	if (sdlen == 1 && *p == '-') {
		/* NILVALUE */
		p++;
	} else if (sdlen > 1) {
		buffer->sd = strndup(p, sdlen);
		p += sdlen;
	} else {
		DPRINTF(D_DATA, "format error\n");
	}
	if	(*p == '\0') start = p;
	else if (*p == ' ')  start = ++p; /* SP */
	DPRINTF(D_DATA, "Got SD \"%s\"\n", buffer->sd);

	/* and now the message itself
	 * note: move back to last start to check for BOM
	 */
all_syslog_msg:
	p = start;

	/* check for UTF-8-BOM */
	if (IS_BOM(p)) {
		DPRINTF(D_DATA, "UTF-8 BOM\n");
		utf8allowed = true;
		p += 3;
	}

	if (*p != '\0' && !utf8allowed) {
		size_t msglen;

		msglen = strlen(p);
		assert(!buffer->msg);
		buffer->msg = copy_utf8_ascii(p, msglen);
		buffer->msgorig = buffer->msg;
		buffer->msglen = buffer->msgsize = strlen(buffer->msg)+1;
	} else if (*p != '\0' && utf8allowed) {
		while (*p != '\0') {
			i = valid_utf8(p);
			if (i == 0)
				*p++ = '?';
			else if (i == 1)
				*p = FORCE2ASCII(*p);
			p += i;
		}
		assert(p != start);
		assert(!buffer->msg);
		buffer->msg = strndup(start, p - start);
		buffer->msgorig = buffer->msg;
		buffer->msglen = buffer->msgsize = 1 + p - start;
	}
	DPRINTF(D_DATA, "Got msg \"%s\"\n", buffer->msg);

	buffer->recvhost = strdup(hname);
	buffer->pri = pri;
	buffer->flags = flags;

	return buffer;
}

/* copies an input into a new ASCII buffer
 * ASCII controls are converted to format "^X"
 * multi-byte UTF-8 chars are converted to format "<ab><cd>"
 */
#define INIT_BUFSIZE 512
char *
copy_utf8_ascii(char *p, size_t p_len)
{
	size_t idst = 0, isrc = 0, dstsize = INIT_BUFSIZE, i;
	char *dst, *tmp_dst;

	MALLOC(dst, dstsize);
	while (isrc < p_len) {
		if (dstsize < idst + 10) {
			/* check for enough space for \0 and a UTF-8
			 * conversion; longest possible is <U+123456> */
			tmp_dst = realloc(dst, dstsize + INIT_BUFSIZE);
			if (!tmp_dst)
				break;
			dst = tmp_dst;
			dstsize += INIT_BUFSIZE;
		}

		i = valid_utf8(&p[isrc]);
		if (i == 0) { /* invalid encoding */
			dst[idst++] = '?';
			isrc++;
		} else if (i == 1) { /* check printable */
			if (iscntrl((unsigned char)p[isrc])
			 && p[isrc] != '\t') {
				if (p[isrc] == '\n') {
					dst[idst++] = ' ';
					isrc++;
				} else {
					dst[idst++] = '^';
					dst[idst++] = p[isrc++] ^ 0100;
				}
			} else
				dst[idst++] = p[isrc++];
		} else {  /* convert UTF-8 to ASCII */
			dst[idst++] = '<';
			idst += snprintf(&dst[idst], dstsize - idst, "U+%x",
			    get_utf8_value(&p[isrc]));
			isrc += i;
			dst[idst++] = '>';
		}
	}
	dst[idst] = '\0';

	/* shrink buffer to right size */
	tmp_dst = realloc(dst, idst+1);
	if (tmp_dst)
		return tmp_dst;
	else
		return dst;
}

struct buf_msg *
printline_bsdsyslog(const char *hname, char *msg,
	int flags, int pri)
{
	struct buf_msg *buffer;
	char *p, *start;
	unsigned msgidlen = 0, sdlen = 0;

	DPRINTF((D_CALL|D_BUFFER|D_DATA), "printline_bsdsyslog("
		"\"%s\", \"%s\", %d, %d)\n", hname, msg, flags, pri);

	buffer = buf_msg_new(0);
	p = msg;
	p += check_timestamp((unsigned char*) p,
		&buffer->timestamp, false, !BSDOutputFormat);
	DPRINTF(D_DATA, "Got timestamp \"%s\"\n", buffer->timestamp);

	if (flags & ADDDATE || !buffer->timestamp) {
		FREEPTR(buffer->timestamp);
		buffer->timestamp = make_timestamp(NULL, !BSDOutputFormat, 0);
	}

	if (*p == ' ') p++; /* SP */
	else goto all_bsd_msg;
	/* in any error case we skip header parsing and
	 * treat all following data as message content */

	/* extract host */
	for (start = p;; p++) {
		if (*p == ' ' || *p == '\0') {
			buffer->host = strndup(start, p - start);
			break;
		} else if (*p == '[' || (*p == ':'
			&& (*(p+1) == ' ' || *(p+1) == '\0'))) {
			/* no host in message */
			buffer->host = LocalFQDN;
			buffer->prog = strndup(start, p - start);
			break;
		} else {
			*p = FORCE2ASCII(*p);
		}
	}
	DPRINTF(D_DATA, "Got host \"%s\"\n", buffer->host);
	/* p @ SP after host, or @ :/[ after prog */

	/* extract program */
	if (!buffer->prog) {
		if (*p == ' ') p++; /* SP */
		else goto all_bsd_msg;

		for (start = p;; p++) {
			if (*p == ' ' || *p == '\0') { /* error */
				goto all_bsd_msg;
			} else if (*p == '[' || (*p == ':'
				&& (*(p+1) == ' ' || *(p+1) == '\0'))) {
				buffer->prog = strndup(start, p - start);
				break;
			} else {
				*p = FORCE2ASCII(*p);
			}
		}
	}
	DPRINTF(D_DATA, "Got prog \"%s\"\n", buffer->prog);
	start = p;

	/* p @ :/[ after prog */
	if (*p == '[') {
		p++;
		if (*p == ' ') p++; /* SP */
		for (start = p;; p++) {
			if (*p == ' ' || *p == '\0') { /* error */
				goto all_bsd_msg;
			} else if (*p == ']') {
				buffer->pid = strndup(start, p - start);
				break;
			} else {
				*p = FORCE2ASCII(*p);
			}
		}
	}
	DPRINTF(D_DATA, "Got pid \"%s\"\n", buffer->pid);

	if (*p == ']') p++;
	if (*p == ':') p++;
	if (*p == ' ') p++;

	/* p @ msgid, @ opening [ of SD or @ first byte of message
	 * accept either case and try to detect MSGID and SD fields
	 *
	 * only limitation: we do not accept UTF-8 data in
	 * BSD Syslog messages -- so all SD values are ASCII-filtered
	 *
	 * I have found one scenario with 'unexpected' behaviour:
	 * if there is only a SD intended, but a) it is short enough
	 * to be a MSGID and b) the first word of the message can also
	 * be parsed as an SD.
	 * example:
	 * "<35>Jul  6 12:39:08 tag[123]: [exampleSDID@0] - hello"
	 * --> parsed as
	 *     MSGID = "[exampleSDID@0]"
	 *     SD    = "-"
	 *     MSG   = "hello"
	 */
	start = p;
	msgidlen = check_msgid(p);
	if (msgidlen) /* check for SD in 2nd field */
		sdlen = check_sd(p+msgidlen+1);

	if (msgidlen && sdlen) {
		/* MSGID in 1st and SD in 2nd field
		 * now check for NILVALUEs and copy */
		if (msgidlen == 1 && *p == '-') {
			p++; /* - */
			p++; /* SP */
			DPRINTF(D_DATA, "Got MSGID \"-\"\n");
		} else {
			/* only has ASCII chars after check_msgid() */
			buffer->msgid = strndup(p, msgidlen);
			p += msgidlen;
			p++; /* SP */
			DPRINTF(D_DATA, "Got MSGID \"%s\"\n",
				buffer->msgid);
		}
	} else {
		/* either no msgid or no SD in 2nd field
		 * --> check 1st field for SD */
		DPRINTF(D_DATA, "No MSGID\n");
		sdlen = check_sd(p);
	}

	if (sdlen == 0) {
		DPRINTF(D_DATA, "No SD\n");
	} else if (sdlen > 1) {
		buffer->sd = copy_utf8_ascii(p, sdlen);
		DPRINTF(D_DATA, "Got SD \"%s\"\n", buffer->sd);
	} else if (sdlen == 1 && *p == '-') {
		p++;
		DPRINTF(D_DATA, "Got SD \"-\"\n");
	} else {
		DPRINTF(D_DATA, "Error\n");
	}

	if (*p == ' ') p++;
	start = p;
	/* and now the message itself
	 * note: do not reset start, because we might come here
	 * by goto and want to have the incomplete field as part
	 * of the msg
	 */
all_bsd_msg:
	if (*p != '\0') {
		size_t msglen = strlen(p);
		buffer->msg = copy_utf8_ascii(p, msglen);
		buffer->msgorig = buffer->msg;
		buffer->msglen = buffer->msgsize = strlen(buffer->msg)+1;
	}
	DPRINTF(D_DATA, "Got msg \"%s\"\n", buffer->msg);

	buffer->recvhost = strdup(hname);
	buffer->pri = pri;
	buffer->flags = flags | BSDSYSLOG;

	return buffer;
}

struct buf_msg *
printline_kernelprintf(const char *hname, char *msg,
	int flags, int pri)
{
	struct buf_msg *buffer;
	char *p;
	unsigned sdlen = 0;

	DPRINTF((D_CALL|D_BUFFER|D_DATA), "printline_kernelprintf("
		"\"%s\", \"%s\", %d, %d)\n", hname, msg, flags, pri);

	buffer = buf_msg_new(0);
	buffer->timestamp = make_timestamp(NULL, !BSDOutputFormat, 0);
	buffer->pri = pri;
	buffer->flags = flags;

	/* assume there is no MSGID but there might be SD */
	p = msg;
	sdlen = check_sd(p);

	if (sdlen == 0) {
		DPRINTF(D_DATA, "No SD\n");
	} else if (sdlen > 1) {
		buffer->sd = copy_utf8_ascii(p, sdlen);
		DPRINTF(D_DATA, "Got SD \"%s\"\n", buffer->sd);
	} else if (sdlen == 1 && *p == '-') {
		p++;
		DPRINTF(D_DATA, "Got SD \"-\"\n");
	} else {
		DPRINTF(D_DATA, "Error\n");
	}

	if (*p == ' ') p++;
	if (*p != '\0') {
		size_t msglen = strlen(p);
		buffer->msg = copy_utf8_ascii(p, msglen);
		buffer->msgorig = buffer->msg;
		buffer->msglen = buffer->msgsize = strlen(buffer->msg)+1;
	}
	DPRINTF(D_DATA, "Got msg \"%s\"\n", buffer->msg);

	return buffer;
}

/*
 * Take a raw input line, read priority and version, call the
 * right message parsing function, then call logmsg().
 */
void
printline(const char *hname, char *msg, int flags)
{
	struct buf_msg *buffer;
	int pri;
	char *p, *q;
	long n;
	bool bsdsyslog = true;

	DPRINTF((D_CALL|D_BUFFER|D_DATA),
		"printline(\"%s\", \"%s\", %d)\n", hname, msg, flags);

	/* test for special codes */
	pri = DEFUPRI;
	p = msg;
	if (*p == '<') {
		errno = 0;
		n = strtol(p + 1, &q, 10);
		if (*q == '>' && n >= 0 && n < INT_MAX && errno == 0) {
			p = q + 1;
			pri = (int)n;
			/* check for syslog-protocol version */
			if (*p == '1' && p[1] == ' ') {
				p += 2;	 /* skip version and space */
				bsdsyslog = false;
			} else {
				bsdsyslog = true;
			}
		}
	}
	if (pri & ~(LOG_FACMASK|LOG_PRIMASK))
		pri = DEFUPRI;

	/*
	 * Don't allow users to log kernel messages.
	 * NOTE: Since LOG_KERN == 0, this will also match
	 *	 messages with no facility specified.
	 */
	if ((pri & LOG_FACMASK) == LOG_KERN)
		pri = LOG_MAKEPRI(LOG_USER, LOG_PRI(pri));

	if (bsdsyslog) {
		buffer = printline_bsdsyslog(hname, p, flags, pri);
	} else {
		buffer = printline_syslogprotocol(hname, p, flags, pri);
	}
	logmsg(buffer);
	DELREF(buffer);
}

/*
 * Take a raw input line from /dev/klog, split and format similar to syslog().
 */
void
printsys(char *msg)
{
	int n, is_printf, pri, flags;
	char *p, *q;
	struct buf_msg *buffer;

	klog_linebufoff = 0;
	for (p = msg; *p != '\0'; ) {
		bool bsdsyslog = true;

		is_printf = 1;
		flags = ISKERNEL | ADDDATE | BSDSYSLOG;
		if (SyncKernel)
			flags |= SYNC_FILE;
		if (is_printf) /* kernel printf's come out on console */
			flags |= IGN_CONS;
		pri = DEFSPRI;

		if (*p == '<') {
			errno = 0;
			n = (int)strtol(p + 1, &q, 10);
			if (*q == '>' && n >= 0 && n < INT_MAX && errno == 0) {
				p = q + 1;
				is_printf = 0;
				pri = n;
				if (*p == '1') { /* syslog-protocol version */
					p += 2;	 /* skip version and space */
					bsdsyslog = false;
				} else {
					bsdsyslog = true;
				}
			}
		}
		for (q = p; *q != '\0' && *q != '\n'; q++)
			/* look for end of line; no further checks.
			 * trust the kernel to send ASCII only */;
		if (*q != '\0')
			*q++ = '\0';
		else {
			memcpy(linebuf, p, klog_linebufoff = q - p);
			break;
		}

		if (pri &~ (LOG_FACMASK|LOG_PRIMASK))
			pri = DEFSPRI;

		/* allow all kinds of input from kernel */
		if (is_printf)
			buffer = printline_kernelprintf(
			    LocalFQDN, p, flags, pri);
		else {
			if (bsdsyslog)
				buffer = printline_bsdsyslog(
				    LocalFQDN, p, flags, pri);
			else
				buffer = printline_syslogprotocol(
				    LocalFQDN, p, flags, pri);
		}

		/* set fields left open */
		if (!buffer->prog)
			buffer->prog = strdup(_PATH_UNIX);
		if (!buffer->host)
			buffer->host = LocalFQDN;
		if (!buffer->recvhost)
			buffer->recvhost = LocalFQDN;

		logmsg(buffer);
		DELREF(buffer);
		p = q;
	}
}

/*
 * Check to see if `name' matches the provided specification, using the
 * specified strstr function.
 */
int
matches_spec(const char *name, const char *spec,
    char *(*check)(const char *, const char *))
{
	const char *s;
	const char *cursor;
	char prev, next;
	size_t len;

	if (name[0] == '\0')
		return 0;

	if (strchr(name, ',')) /* sanity */
		return 0;

	len = strlen(name);
	cursor = spec;
	while ((s = (*check)(cursor, name)) != NULL) {
		prev = s == spec ? ',' : *(s - 1);
		cursor = s + len;
		next = *cursor;

		if (prev == ',' && (next == '\0' || next == ','))
			return 1;
	}

	return 0;
}

/*
 * wrapper with old function signature,
 * keeps calling code shorter and hides buffer allocation
 */
void
logmsg_async(int pri, const char *sd, const char *msg, int flags)
{
	struct buf_msg *buffer;
	size_t msglen;

	DPRINTF((D_CALL|D_DATA), "logmsg_async(%d, \"%s\", \"%s\", %d)\n",
	    pri, sd, msg, flags);

	if (msg) {
		msglen = strlen(msg);
		msglen++;		/* adds \0 */
		buffer = buf_msg_new(msglen);
		buffer->msglen = strlcpy(buffer->msg, msg, msglen) + 1;
	} else {
		buffer = buf_msg_new(0);
	}
	if (sd) buffer->sd = strdup(sd);
	buffer->timestamp = make_timestamp(NULL, !BSDOutputFormat, 0);
	buffer->prog = appname;
	buffer->pid = include_pid;
	buffer->recvhost = buffer->host = LocalFQDN;
	buffer->pri = pri;
	buffer->flags = flags;

	logmsg(buffer);
	DELREF(buffer);
}

/* read timestamp in from_buf, convert into a timestamp in to_buf
 *
 * returns length of timestamp found in from_buf (= number of bytes consumed)
 */
size_t
check_timestamp(unsigned char *from_buf, char **to_buf,
	bool from_iso, bool to_iso)
{
	unsigned char *q;
	int p;
	bool found_ts = false;

	DPRINTF((D_CALL|D_DATA), "check_timestamp(%p = \"%s\", from_iso=%d, "
	    "to_iso=%d)\n", from_buf, from_buf, from_iso, to_iso);

	if (!from_buf) return 0;
	/*
	 * Check to see if msg looks non-standard.
	 * looks at every char because we do not have a msg length yet
	 */
	/* detailed checking adapted from Albert Mietus' sl_timestamp.c */
	if (from_iso) {
		if (from_buf[4] == '-' && from_buf[7] == '-'
		    && from_buf[10] == 'T' && from_buf[13] == ':'
		    && from_buf[16] == ':'
		    && isdigit(from_buf[0]) && isdigit(from_buf[1])
		    && isdigit(from_buf[2]) && isdigit(from_buf[3])  /* YYYY */
		    && isdigit(from_buf[5]) && isdigit(from_buf[6])
		    && isdigit(from_buf[8]) && isdigit(from_buf[9])  /* mm dd */
		    && isdigit(from_buf[11]) && isdigit(from_buf[12]) /* HH */
		    && isdigit(from_buf[14]) && isdigit(from_buf[15]) /* MM */
		    && isdigit(from_buf[17]) && isdigit(from_buf[18]) /* SS */
		    )  {
			/* time-secfrac */
			if (from_buf[19] == '.')
				for (p=20; isdigit(from_buf[p]); p++) /* NOP*/;
			else
				p = 19;
			/* time-offset */
			if (from_buf[p] == 'Z'
			 || ((from_buf[p] == '+' || from_buf[p] == '-')
			    && from_buf[p+3] == ':'
			    && isdigit(from_buf[p+1]) && isdigit(from_buf[p+2])
			    && isdigit(from_buf[p+4]) && isdigit(from_buf[p+5])
			 ))
				found_ts = true;
		}
	} else {
		if (from_buf[3] == ' ' && from_buf[6] == ' '
		    && from_buf[9] == ':' && from_buf[12] == ':'
		    && (from_buf[4] == ' ' || isdigit(from_buf[4]))
		    && isdigit(from_buf[5]) /* dd */
		    && isdigit(from_buf[7])  && isdigit(from_buf[8])   /* HH */
		    && isdigit(from_buf[10]) && isdigit(from_buf[11])  /* MM */
		    && isdigit(from_buf[13]) && isdigit(from_buf[14])  /* SS */
		    && isupper(from_buf[0]) && islower(from_buf[1]) /* month */
		    && islower(from_buf[2]))
			found_ts = true;
	}
	if (!found_ts) {
		if (from_buf[0] == '-' && from_buf[1] == ' ') {
			/* NILVALUE */
			if (to_iso) {
				/* with ISO = syslog-protocol output leave
			 	 * it as is, because it is better to have
			 	 * no timestamp than a wrong one.
			 	 */
				*to_buf = strdup("-");
			} else {
				/* with BSD Syslog the field is reqired
				 * so replace it with current time
				 */
				*to_buf = make_timestamp(NULL, false, 0);
			}
			return 2;
		}
		*to_buf = make_timestamp(NULL, false, 0);
		return 0;
	}

	if (!from_iso && !to_iso) {
		/* copy BSD timestamp */
		DPRINTF(D_CALL, "check_timestamp(): copy BSD timestamp\n");
		*to_buf = strndup((char *)from_buf, BSD_TIMESTAMPLEN);
		return BSD_TIMESTAMPLEN;
	} else if (from_iso && to_iso) {
		/* copy ISO timestamp */
		DPRINTF(D_CALL, "check_timestamp(): copy ISO timestamp\n");
		if (!(q = (unsigned char *) strchr((char *)from_buf, ' ')))
			q = from_buf + strlen((char *)from_buf);
		*to_buf = strndup((char *)from_buf, q - from_buf);
		return q - from_buf;
	} else if (from_iso && !to_iso) {
		/* convert ISO->BSD */
		struct tm parsed;
		time_t timeval;
		char tsbuf[MAX_TIMESTAMPLEN];
		int i = 0;

		DPRINTF(D_CALL, "check_timestamp(): convert ISO->BSD\n");
		for(i = 0; i < MAX_TIMESTAMPLEN && from_buf[i] != '\0'
		    && from_buf[i] != '.' && from_buf[i] != ' '; i++)
			tsbuf[i] = from_buf[i]; /* copy date & time */
		for(; i < MAX_TIMESTAMPLEN && from_buf[i] != '\0'
		    && from_buf[i] != '+' && from_buf[i] != '-'
		    && from_buf[i] != 'Z' && from_buf[i] != ' '; i++)
			;			   /* skip fraction digits */
		for(; i < MAX_TIMESTAMPLEN && from_buf[i] != '\0'
		    && from_buf[i] != ':' && from_buf[i] != ' ' ; i++)
			tsbuf[i] = from_buf[i]; /* copy TZ */
		if (from_buf[i] == ':') i++;	/* skip colon */
		for(; i < MAX_TIMESTAMPLEN && from_buf[i] != '\0'
		    && from_buf[i] != ' ' ; i++)
			tsbuf[i] = from_buf[i]; /* copy TZ */

		(void)memset(&parsed, 0, sizeof(parsed));
		parsed.tm_isdst = -1;
		(void)strptime(tsbuf, "%FT%T%z", &parsed);
		timeval = mktime(&parsed);

		*to_buf = make_timestamp(&timeval, false, BSD_TIMESTAMPLEN);
		return i;
	} else if (!from_iso && to_iso) {
		/* convert BSD->ISO */
		struct tm parsed;
		struct tm *current;
		time_t timeval;

		(void)memset(&parsed, 0, sizeof(parsed));
		parsed.tm_isdst = -1;
		DPRINTF(D_CALL, "check_timestamp(): convert BSD->ISO\n");
		strptime((char *)from_buf, "%b %d %T", &parsed);
		current = gmtime(&now);

		/* use current year and timezone */
		parsed.tm_isdst = current->tm_isdst;
		parsed.tm_gmtoff = current->tm_gmtoff;
		parsed.tm_year = current->tm_year;
		if (current->tm_mon == 0 && parsed.tm_mon == 11)
			parsed.tm_year--;

		timeval = mktime(&parsed);
		*to_buf = make_timestamp(&timeval, true, MAX_TIMESTAMPLEN - 1);

		return BSD_TIMESTAMPLEN;
	} else {
		DPRINTF(D_MISC,
			"Executing unreachable code in check_timestamp()\n");
		return 0;
	}
}

/*
 * Log a message to the appropriate log files, users, etc. based on
 * the priority.
 */
void
logmsg(struct buf_msg *buffer)
{
	struct filed *f;
	int fac, omask, prilev;

	DPRINTF((D_CALL|D_BUFFER), "logmsg: buffer@%p, pri 0%o/%d, flags 0x%x,"
	    " timestamp \"%s\", from \"%s\", sd \"%s\", msg \"%s\"\n",
	    buffer, buffer->pri, buffer->pri, buffer->flags,
	    buffer->timestamp, buffer->recvhost, buffer->sd, buffer->msg);

	omask = sigblock(sigmask(SIGHUP)|sigmask(SIGALRM));

	/* sanity check */
	assert(buffer->refcount == 1);
	assert(buffer->msglen <= buffer->msgsize);
	assert(buffer->msgorig <= buffer->msg);
	assert((buffer->msg && buffer->msglen == strlen(buffer->msg)+1)
	      || (!buffer->msg && !buffer->msglen));
	if (!buffer->msg && !buffer->sd && !buffer->msgid)
		DPRINTF(D_BUFFER, "Empty message?\n");

	/* extract facility and priority level */
	if (buffer->flags & MARK)
		fac = LOG_NFACILITIES;
	else
		fac = LOG_FAC(buffer->pri);
	prilev = LOG_PRI(buffer->pri);

	/* log the message to the particular outputs */
	if (!Initialized) {
		f = &consfile;
		f->f_file = open(ctty, O_WRONLY | O_NDELAY, 0);

		if (f->f_file >= 0) {
			DELREF(f->f_prevmsg);
			f->f_prevmsg = NEWREF(buffer);
			fprintlog(f, NEWREF(buffer), NULL);
			DELREF(buffer);
			(void)close(f->f_file);
		}
		(void)sigsetmask(omask);
		return;
	}

	for (f = Files; f; f = f->f_next) {
		char *h;	/* host to use for comparing */

		/* skip messages that are incorrect priority */
		if (!MATCH_PRI(f, fac, prilev)
		    || f->f_pmask[fac] == INTERNAL_NOPRI)
			continue;

		/* skip messages with the incorrect host name */
		/* compare with host (which is supposedly more correct), */
		/* but fallback to recvhost if host is NULL */
		h = (buffer->host != NULL) ? buffer->host : buffer->recvhost;
		if (f->f_host != NULL && h != NULL) {
			char shost[MAXHOSTNAMELEN + 1];

			if (BSDOutputFormat) {
				(void)strlcpy(shost, h, sizeof(shost));
				trim_anydomain(shost);
				h = shost;
			}
			switch (f->f_host[0]) {
			case '+':
				if (! matches_spec(h, f->f_host + 1,
				    strcasestr))
					continue;
				break;
			case '-':
				if (matches_spec(h, f->f_host + 1,
				    strcasestr))
					continue;
				break;
			}
		}

		/* skip messages with the incorrect program name */
		if (f->f_program != NULL && buffer->prog != NULL) {
			switch (f->f_program[0]) {
			case '+':
				if (!matches_spec(buffer->prog,
				    f->f_program + 1, strstr))
					continue;
				break;
			case '-':
				if (matches_spec(buffer->prog,
				    f->f_program + 1, strstr))
					continue;
				break;
			default:
				if (!matches_spec(buffer->prog,
				    f->f_program, strstr))
					continue;
				break;
			}
		}

		if (f->f_type == F_CONSOLE && (buffer->flags & IGN_CONS))
			continue;

		/* don't output marks to recently written files */
		if ((buffer->flags & MARK)
		 && (now - f->f_time) < MarkInterval / 2)
			continue;

		/*
		 * suppress duplicate lines to this file unless NoRepeat
		 */
#define MSG_FIELD_EQ(x) ((!buffer->x && !f->f_prevmsg->x) ||	\
    (buffer->x && f->f_prevmsg->x && !strcmp(buffer->x, f->f_prevmsg->x)))

		if ((buffer->flags & MARK) == 0 &&
		    f->f_prevmsg &&
		    buffer->msglen == f->f_prevmsg->msglen &&
		    !NoRepeat &&
		    MSG_FIELD_EQ(host) &&
		    MSG_FIELD_EQ(sd) &&
		    MSG_FIELD_EQ(msg)
		    ) {
			f->f_prevcount++;
			DPRINTF(D_DATA, "Msg repeated %d times, %ld sec of %d\n",
			    f->f_prevcount, (long)(now - f->f_time),
			    repeatinterval[f->f_repeatcount]);
			/*
			 * If domark would have logged this by now,
			 * flush it now (so we don't hold isolated messages),
			 * but back off so we'll flush less often
			 * in the future.
			 */
			if (now > REPEATTIME(f)) {
				fprintlog(f, NEWREF(buffer), NULL);
				DELREF(buffer);
				BACKOFF(f);
			}
		} else {
			/* new line, save it */
			if (f->f_prevcount)
				fprintlog(f, NULL, NULL);
			f->f_repeatcount = 0;
			DELREF(f->f_prevmsg);
			f->f_prevmsg = NEWREF(buffer);
			fprintlog(f, NEWREF(buffer), NULL);
			DELREF(buffer);
		}
	}
	(void)sigsetmask(omask);
}

/*
 * format one buffer into output format given by flag BSDOutputFormat
 * line is allocated and has to be free()d by caller
 * size_t pointers are optional, if not NULL then they will return
 *   different lenghts used for formatting and output
 */
#define OUT(x) ((x)?(x):"-")
bool
format_buffer(struct buf_msg *buffer, char **line, size_t *ptr_linelen,
	size_t *ptr_msglen, size_t *ptr_tlsprefixlen, size_t *ptr_prilen)
{
#define FPBUFSIZE 30
	static char ascii_empty[] = "";
	char fp_buf[FPBUFSIZE] = "\0";
	char *hostname, *shorthostname = NULL;
	char *ascii_sd = ascii_empty;
	char *ascii_msg = ascii_empty;
	size_t linelen, msglen, tlsprefixlen, prilen, j;

	DPRINTF(D_CALL, "format_buffer(%p)\n", buffer);
	if (!buffer) return false;

	/* All buffer fields are set with strdup(). To avoid problems
	 * on memory exhaustion we allow them to be empty and replace
	 * the essential fields with already allocated generic values.
	 */
	if (!buffer->timestamp)
		buffer->timestamp = timestamp;
	if (!buffer->host && !buffer->recvhost)
		buffer->host = LocalFQDN;

	if (LogFacPri) {
		const char *f_s = NULL, *p_s = NULL;
		int fac = buffer->pri & LOG_FACMASK;
		int pri = LOG_PRI(buffer->pri);
		char f_n[5], p_n[5];

		if (LogFacPri > 1) {
			CODE *c;

			for (c = facilitynames; c->c_name != NULL; c++) {
				if (c->c_val == fac) {
					f_s = c->c_name;
					break;
				}
			}
			for (c = prioritynames; c->c_name != NULL; c++) {
				if (c->c_val == pri) {
					p_s = c->c_name;
					break;
				}
			}
		}
		if (f_s == NULL) {
			snprintf(f_n, sizeof(f_n), "%d", LOG_FAC(fac));
			f_s = f_n;
		}
		if (p_s == NULL) {
			snprintf(p_n, sizeof(p_n), "%d", pri);
			p_s = p_n;
		}
		snprintf(fp_buf, sizeof(fp_buf), "<%s.%s>", f_s, p_s);
	}

	/* hostname or FQDN */
	hostname = (buffer->host ? buffer->host : buffer->recvhost);
	if (BSDOutputFormat
	 && (shorthostname = strdup(hostname))) {
		/* if the previous BSD output format with "host [recvhost]:"
		 * gets implemented, this is the right place to distinguish
		 * between buffer->host and buffer->recvhost
		 */
		trim_anydomain(shorthostname);
		hostname = shorthostname;
	}

	/* new message formatting:
	 * instead of using iov always assemble one complete TLS-ready line
	 * with length and priority (depending on BSDOutputFormat either in
	 * BSD Syslog or syslog-protocol format)
	 *
	 * additionally save the length of the prefixes,
	 * so UDP destinations can skip the length prefix and
	 * file/pipe/wall destinations can omit length and priority
	 */
	/* first determine required space */
	if (BSDOutputFormat) {
		/* only output ASCII chars */
		if (buffer->sd)
			ascii_sd = copy_utf8_ascii(buffer->sd,
				strlen(buffer->sd));
		if (buffer->msg) {
			if (IS_BOM(buffer->msg))
				ascii_msg = copy_utf8_ascii(buffer->msg,
					buffer->msglen - 1);
			else /* assume already converted at input */
				ascii_msg = buffer->msg;
		}
		msglen = snprintf(NULL, 0, "<%d>%s%.15s %s %s%s%s%s: %s%s%s",
			     buffer->pri, fp_buf, buffer->timestamp,
			     hostname, OUT(buffer->prog),
			     buffer->pid ? "[" : "",
			     buffer->pid ? buffer->pid : "",
			     buffer->pid ? "]" : "", ascii_sd,
			     (buffer->sd && buffer->msg ? " ": ""), ascii_msg);
	} else
		msglen = snprintf(NULL, 0, "<%d>1 %s%s %s %s %s %s %s%s%s",
			     buffer->pri, fp_buf, buffer->timestamp,
			     hostname, OUT(buffer->prog), OUT(buffer->pid),
			     OUT(buffer->msgid), OUT(buffer->sd),
			     (buffer->msg ? " ": ""),
			     (buffer->msg ? buffer->msg: ""));
	/* add space for length prefix */
	tlsprefixlen = 0;
	for (j = msglen; j; j /= 10)
		tlsprefixlen++;
	/* one more for the space */
	tlsprefixlen++;

	prilen = snprintf(NULL, 0, "<%d>", buffer->pri);
	if (!BSDOutputFormat)
		prilen += 2; /* version char and space */
	MALLOC(*line, msglen + tlsprefixlen + 1);
	if (BSDOutputFormat)
		linelen = snprintf(*line,
		     msglen + tlsprefixlen + 1,
		     "%zu <%d>%s%.15s %s %s%s%s%s: %s%s%s",
		     msglen, buffer->pri, fp_buf, buffer->timestamp,
		     hostname, OUT(buffer->prog),
		     (buffer->pid ? "[" : ""),
		     (buffer->pid ? buffer->pid : ""),
		     (buffer->pid ? "]" : ""), ascii_sd,
		     (buffer->sd && buffer->msg ? " ": ""), ascii_msg);
	else
		linelen = snprintf(*line,
		     msglen + tlsprefixlen + 1,
		     "%zu <%d>1 %s%s %s %s %s %s %s%s%s",
		     msglen, buffer->pri, fp_buf, buffer->timestamp,
		     hostname, OUT(buffer->prog), OUT(buffer->pid),
		     OUT(buffer->msgid), OUT(buffer->sd),
		     (buffer->msg ? " ": ""),
		     (buffer->msg ? buffer->msg: ""));
	DPRINTF(D_DATA, "formatted %zu octets to: '%.*s' (linelen %zu, "
	    "msglen %zu, tlsprefixlen %zu, prilen %zu)\n", linelen,
	    (int)linelen, *line, linelen, msglen, tlsprefixlen, prilen);

	FREEPTR(shorthostname);
	if (ascii_sd != ascii_empty)
		FREEPTR(ascii_sd);
	if (ascii_msg != ascii_empty && ascii_msg != buffer->msg)
		FREEPTR(ascii_msg);

	if (ptr_linelen)      *ptr_linelen	= linelen;
	if (ptr_msglen)	      *ptr_msglen	= msglen;
	if (ptr_tlsprefixlen) *ptr_tlsprefixlen = tlsprefixlen;
	if (ptr_prilen)	      *ptr_prilen	= prilen;
	return true;
}

/*
 * if qentry == NULL: new message, if temporarily undeliverable it will be enqueued
 * if qentry != NULL: a temporarily undeliverable message will not be enqueued,
 *		    but after delivery be removed from the queue
 */
void
fprintlog(struct filed *f, struct buf_msg *passedbuffer, struct buf_queue *qentry)
{
	static char crnl[] = "\r\n";
	struct buf_msg *buffer = passedbuffer;
	struct iovec iov[4];
	struct iovec *v = iov;
	bool error = false;
	int e = 0, len = 0;
	size_t msglen, linelen, tlsprefixlen, prilen;
	char *p, *line = NULL, *lineptr = NULL;
#ifndef DISABLE_SIGN
	bool newhash = false;
#endif
#define REPBUFSIZE 80
	char greetings[200];
#define ADDEV() do { v++; assert((size_t)(v - iov) < A_CNT(iov)); } while(/*CONSTCOND*/0)

	DPRINTF(D_CALL, "fprintlog(%p, %p, %p)\n", f, buffer, qentry);

	f->f_time = now;

	/* increase refcount here and lower again at return.
	 * this enables the buffer in the else branch to be freed
	 * --> every branch needs one NEWREF() or buf_msg_new()! */
	if (buffer) {
		(void)NEWREF(buffer);
	} else {
		if (f->f_prevcount > 1) {
			/* possible syslog-sign incompatibility:
			 * assume destinations f1 and f2 share one SG and
			 * get the same message sequence.
			 *
			 * now both f1 and f2 generate "repeated" messages
			 * "repeated" messages are different due to different
			 * timestamps
			 * the SG will get hashes for the two "repeated" messages
			 *
			 * now both f1 and f2 are just fine, but a verification
			 * will report that each 'lost' a message, i.e. the
			 * other's "repeated" message
			 *
			 * conditions for 'safe configurations':
			 * - use NoRepeat option,
			 * - use SG 3, or
			 * - have exactly one destination for every PRI
			 */
			buffer = buf_msg_new(REPBUFSIZE);
			buffer->msglen = snprintf(buffer->msg, REPBUFSIZE,
			    "last message repeated %d times", f->f_prevcount);
			buffer->timestamp = make_timestamp(NULL,
			    !BSDOutputFormat, 0);
			buffer->pri = f->f_prevmsg->pri;
			buffer->host = LocalFQDN;
			buffer->prog = appname;
			buffer->pid = include_pid;

		} else {
			buffer = NEWREF(f->f_prevmsg);
		}
	}

	/* no syslog-sign messages to tty/console/... */
	if ((buffer->flags & SIGN_MSG)
	    && ((f->f_type == F_UNUSED)
	    || (f->f_type == F_TTY)
	    || (f->f_type == F_CONSOLE)
	    || (f->f_type == F_USERS)
	    || (f->f_type == F_WALL)
	    || (f->f_type == F_FIFO))) {
		DELREF(buffer);
		return;
	}

	/* buffering works only for few types */
	if (qentry
	    && (f->f_type != F_TLS)
	    && (f->f_type != F_PIPE)
	    && (f->f_type != F_FILE)
	    && (f->f_type != F_FIFO)) {
		errno = 0;
		logerror("Warning: unexpected message type %d in buffer",
		    f->f_type);
		DELREF(buffer);
		return;
	}

	if (!format_buffer(buffer, &line,
	    &linelen, &msglen, &tlsprefixlen, &prilen)) {
		DPRINTF(D_CALL, "format_buffer() failed, skip message\n");
		DELREF(buffer);
		return;
	}
	/* assert maximum message length */
	if (TypeInfo[f->f_type].max_msg_length != -1
	    && (size_t)TypeInfo[f->f_type].max_msg_length
	    < linelen - tlsprefixlen - prilen) {
		linelen = TypeInfo[f->f_type].max_msg_length
		    + tlsprefixlen + prilen;
		DPRINTF(D_DATA, "truncating oversized message to %zu octets\n",
		    linelen);
	}

#ifndef DISABLE_SIGN
	/* keep state between appending the hash (before buffer is sent)
	 * and possibly sending a SB (after buffer is sent): */
	/* get hash */
	if (!(buffer->flags & SIGN_MSG) && !qentry) {
		char *hash = NULL;
		struct signature_group_t *sg;

		if ((sg = sign_get_sg(buffer->pri, f)) != NULL) {
			if (sign_msg_hash(line + tlsprefixlen, &hash))
				newhash = sign_append_hash(hash, sg);
			else
				DPRINTF(D_SIGN,
					"Unable to hash line \"%s\"\n", line);
		}
	}
#endif /* !DISABLE_SIGN */

	/* set start and length of buffer and/or fill iovec */
	switch (f->f_type) {
	case F_UNUSED:
		/* nothing */
		break;
	case F_TLS:
		/* nothing, as TLS uses whole buffer to send */
		lineptr = line;
		len = linelen;
		break;
	case F_FORW:
		lineptr = line + tlsprefixlen;
		len = linelen - tlsprefixlen;
		break;
	case F_PIPE:
	case F_FIFO:
	case F_FILE:  /* fallthrough */
		if (f->f_flags & FFLAG_FULL) {
			v->iov_base = line + tlsprefixlen;
			v->iov_len = linelen - tlsprefixlen;
		} else {
			v->iov_base = line + tlsprefixlen + prilen;
			v->iov_len = linelen - tlsprefixlen - prilen;
		}
		ADDEV();
		v->iov_base = &crnl[1];
		v->iov_len = 1;
		ADDEV();
		break;
	case F_CONSOLE:
	case F_TTY:
		/* filter non-ASCII */
		p = line;
		while (*p) {
			*p = FORCE2ASCII(*p);
			p++;
		}
		v->iov_base = line + tlsprefixlen + prilen;
		v->iov_len = linelen - tlsprefixlen - prilen;
		ADDEV();
		v->iov_base = crnl;
		v->iov_len = 2;
		ADDEV();
		break;
	case F_WALL:
		v->iov_base = greetings;
		v->iov_len = snprintf(greetings, sizeof(greetings),
		    "\r\n\7Message from syslogd@%s at %s ...\r\n",
		    (buffer->host ? buffer->host : buffer->recvhost),
		    buffer->timestamp);
		ADDEV();
	case F_USERS: /* fallthrough */
		/* filter non-ASCII */
		p = line;
		while (*p) {
			*p = FORCE2ASCII(*p);
			p++;
		}
		v->iov_base = line + tlsprefixlen + prilen;
		v->iov_len = linelen - tlsprefixlen - prilen;
		ADDEV();
		v->iov_base = &crnl[1];
		v->iov_len = 1;
		ADDEV();
		break;
	}

	/* send */
	switch (f->f_type) {
	case F_UNUSED:
		DPRINTF(D_MISC, "Logging to %s\n", TypeInfo[f->f_type].name);
		break;

	case F_FORW:
		DPRINTF(D_MISC, "Logging to %s %s\n",
		    TypeInfo[f->f_type].name, f->f_un.f_forw.f_hname);
		udp_send(f, lineptr, len);
		break;

#ifndef DISABLE_TLS
	case F_TLS:
		DPRINTF(D_MISC, "Logging to %s %s\n",
		    TypeInfo[f->f_type].name,
		    f->f_un.f_tls.tls_conn->hostname);
		/* make sure every message gets queued once
		 * it will be removed when sendmsg is sent and free()d */
		if (!qentry)
			qentry = message_queue_add(f, NEWREF(buffer));
		(void)tls_send(f, lineptr, len, qentry);
		break;
#endif /* !DISABLE_TLS */

	case F_PIPE:
		DPRINTF(D_MISC, "Logging to %s %s\n",
		    TypeInfo[f->f_type].name, f->f_un.f_pipe.f_pname);
		if (f->f_un.f_pipe.f_pid == 0) {
			/* (re-)open */
			if ((f->f_file = p_open(f->f_un.f_pipe.f_pname,
			    &f->f_un.f_pipe.f_pid)) < 0) {
				f->f_type = F_UNUSED;
				logerror("%s", f->f_un.f_pipe.f_pname);
				message_queue_freeall(f);
				break;
			} else if (!qentry) /* prevent recursion */
				SEND_QUEUE(f);
		}
		if (writev(f->f_file, iov, v - iov) < 0) {
			e = errno;
			if (f->f_un.f_pipe.f_pid > 0) {
				(void) close(f->f_file);
				deadq_enter(f->f_un.f_pipe.f_pid,
				    f->f_un.f_pipe.f_pname);
			}
			f->f_un.f_pipe.f_pid = 0;
			/*
			 * If the error was EPIPE, then what is likely
			 * has happened is we have a command that is
			 * designed to take a single message line and
			 * then exit, but we tried to feed it another
			 * one before we reaped the child and thus
			 * reset our state.
			 *
			 * Well, now we've reset our state, so try opening
			 * the pipe and sending the message again if EPIPE
			 * was the error.
			 */
			if (e == EPIPE) {
				if ((f->f_file = p_open(f->f_un.f_pipe.f_pname,
				     &f->f_un.f_pipe.f_pid)) < 0) {
					f->f_type = F_UNUSED;
					logerror("%s", f->f_un.f_pipe.f_pname);
					message_queue_freeall(f);
					break;
				}
				if (writev(f->f_file, iov, v - iov) < 0) {
					e = errno;
					if (f->f_un.f_pipe.f_pid > 0) {
					    (void) close(f->f_file);
					    deadq_enter(f->f_un.f_pipe.f_pid,
						f->f_un.f_pipe.f_pname);
					}
					f->f_un.f_pipe.f_pid = 0;
					error = true;	/* enqueue on return */
				} else
					e = 0;
			}
			if (e != 0 && !error) {
				errno = e;
				logerror("%s", f->f_un.f_pipe.f_pname);
			}
		}
		if (e == 0 && qentry) { /* sent buffered msg */
			message_queue_remove(f, qentry);
		}
		break;

	case F_CONSOLE:
		if (buffer->flags & IGN_CONS) {
			DPRINTF(D_MISC, "Logging to %s (ignored)\n",
				TypeInfo[f->f_type].name);
			break;
		}
		/* FALLTHROUGH */

	case F_TTY:
	case F_FILE:
		DPRINTF(D_MISC, "Logging to %s %s\n",
			TypeInfo[f->f_type].name, f->f_un.f_fname);
	again:
		if ((f->f_type == F_FILE ? writev(f->f_file, iov, v - iov) :
		    writev1(f->f_file, iov, v - iov)) < 0) {
			e = errno;
			if (f->f_type == F_FILE && e == ENOSPC) {
				int lasterror = f->f_lasterror;
				f->f_lasterror = e;
				if (lasterror != e)
					logerror("%s", f->f_un.f_fname);
				error = true;	/* enqueue on return */
			}
			(void)close(f->f_file);
			/*
			 * Check for errors on TTY's due to loss of tty
			 */
			if ((e == EIO || e == EBADF) && f->f_type != F_FILE) {
				f->f_file = open(f->f_un.f_fname,
				    O_WRONLY|O_APPEND|O_NONBLOCK, 0);
				if (f->f_file < 0) {
					f->f_type = F_UNUSED;
					logerror("%s", f->f_un.f_fname);
					message_queue_freeall(f);
				} else
					goto again;
			} else {
				f->f_type = F_UNUSED;
				errno = e;
				f->f_lasterror = e;
				logerror("%s", f->f_un.f_fname);
				message_queue_freeall(f);
			}
		} else {
			f->f_lasterror = 0;
			if ((buffer->flags & SYNC_FILE)
			 && (f->f_flags & FFLAG_SYNC))
				(void)fsync(f->f_file);
			/* Problem with files: We cannot check beforehand if
			 * they would be writeable and call send_queue() first.
			 * So we call send_queue() after a successful write,
			 * which means the first message will be out of order.
			 */
			if (!qentry) /* prevent recursion */
				SEND_QUEUE(f);
			else if (qentry) /* sent buffered msg */
				message_queue_remove(f, qentry);
		}
		break;

	case F_FIFO:
		DPRINTF(D_MISC, "Logging to %s %s\n",
			TypeInfo[f->f_type].name, f->f_un.f_fname);
		if (f->f_file < 0) {
			f->f_file =
			  open(f->f_un.f_fname, O_WRONLY|O_NONBLOCK, 0);
			e = errno;
			if (f->f_file < 0 && e == ENXIO) {
				/* Drop messages with no reader */
				if (qentry)
					message_queue_remove(f, qentry);
				break;
			}
		}

		if (f->f_file >= 0 && writev(f->f_file, iov, v - iov) < 0) {
			e = errno;

			/* Enqueue if the fifo buffer is full */
			if (e == EAGAIN) {
				if (f->f_lasterror != e)
					logerror("%s", f->f_un.f_fname);
				f->f_lasterror = e;
				error = true;	/* enqueue on return */
				break;
			}

			close(f->f_file);
			f->f_file = -1;

			/* Drop messages with no reader */
			if (e == EPIPE) {
				if (qentry)
					message_queue_remove(f, qentry);
				break;
			}
		}

		if (f->f_file < 0) {
			f->f_type = F_UNUSED;
			errno = e;
			f->f_lasterror = e;
			logerror("%s", f->f_un.f_fname);
			message_queue_freeall(f);
			break;
		}

		f->f_lasterror = 0;
		if (!qentry) /* prevent recursion (see comment for F_FILE) */
			SEND_QUEUE(f);
		if (qentry) /* sent buffered msg */
			message_queue_remove(f, qentry);
		break;

	case F_USERS:
	case F_WALL:
		DPRINTF(D_MISC, "Logging to %s\n", TypeInfo[f->f_type].name);
		wallmsg(f, iov, v - iov);
		break;
	}
	f->f_prevcount = 0;

	if (error && !qentry)
		message_queue_add(f, NEWREF(buffer));
#ifndef DISABLE_SIGN
	if (newhash) {
		struct signature_group_t *sg;
		sg = sign_get_sg(buffer->pri, f);
		(void)sign_send_signature_block(sg, false);
	}
#endif /* !DISABLE_SIGN */
	/* this belongs to the ad-hoc buffer at the first if(buffer) */
	DELREF(buffer);
	/* TLS frees on its own */
	if (f->f_type != F_TLS)
		FREEPTR(line);
}

/* send one line by UDP */
void
udp_send(struct filed *f, char *line, size_t len)
{
	int lsent, fail, retry, j;
	struct addrinfo *r;

	DPRINTF((D_NET|D_CALL), "udp_send(f=%p, line=\"%s\", "
	    "len=%zu) to dest.\n", f, line, len);

	if (!finet)
		return;

	lsent = -1;
	fail = 0;
	assert(f->f_type == F_FORW);
	for (r = f->f_un.f_forw.f_addr; r; r = r->ai_next) {
		retry = 0;
		for (j = 0; j < finet->fd; j++) {
			if (finet[j+1].af != r->ai_family)
				continue;
sendagain:
			lsent = sendto(finet[j+1].fd, line, len, 0,
			    r->ai_addr, r->ai_addrlen);
			if (lsent == -1) {
				switch (errno) {
				case ENOBUFS:
					/* wait/retry/drop */
					if (++retry < 5) {
						usleep(1000);
						goto sendagain;
					}
					break;
				case EHOSTDOWN:
				case EHOSTUNREACH:
				case ENETDOWN:
					/* drop */
					break;
				default:
					/* busted */
					fail++;
					break;
				}
			} else if ((size_t)lsent == len)
				break;
		}
		if ((size_t)lsent != len && fail) {
			f->f_type = F_UNUSED;
			logerror("sendto() failed");
		}
	}
}

/*
 *  WALLMSG -- Write a message to the world at large
 *
 *	Write the specified message to either the entire
 *	world, or a list of approved users.
 */
void
wallmsg(struct filed *f, struct iovec *iov, size_t iovcnt)
{
#ifdef __NetBSD_Version__
	static int reenter;			/* avoid calling ourselves */
	int i;
	char *p;
	struct utmpentry *ep;

	if (reenter++)
		return;

	(void)getutentries(NULL, &ep);
	/* NOSTRICT */
	for (; ep; ep = ep->next) {
		if (f->f_type == F_WALL) {
			if ((p = ttymsg(iov, iovcnt, ep->line, TTYMSGTIME))
			    != NULL) {
				errno = 0;	/* already in msg */
				logerror("%s", p);
			}
			continue;
		}
		/* should we send the message to this user? */
		for (i = 0; i < MAXUNAMES; i++) {
			if (!f->f_un.f_uname[i][0])
				break;
			if (strcmp(f->f_un.f_uname[i], ep->name) == 0) {
				struct stat st;
				char tty[MAXPATHLEN];
				snprintf(tty, sizeof(tty), "%s/%s", _PATH_DEV,
				    ep->line);
				if (stat(tty, &st) != -1 &&
				    (st.st_mode & S_IWGRP) == 0)
					break;

				if ((p = ttymsg(iov, iovcnt, ep->line,
				    TTYMSGTIME)) != NULL) {
					errno = 0;	/* already in msg */
					logerror("%s", p);
				}
				break;
			}
		}
	}
	reenter = 0;
#endif /* __NetBSD_Version__ */
}

void
/*ARGSUSED*/
reapchild(int fd, short event, void *ev)
{
	int status;
	pid_t pid;
	struct filed *f;

	while ((pid = wait3(&status, WNOHANG, NULL)) > 0) {
		if (!Initialized || ShuttingDown) {
			/*
			 * Be silent while we are initializing or
			 * shutting down.
			 */
			continue;
		}

		if (deadq_remove(pid))
			continue;

		/* Now, look in the list of active processes. */
		for (f = Files; f != NULL; f = f->f_next) {
			if (f->f_type == F_PIPE &&
			    f->f_un.f_pipe.f_pid == pid) {
				(void) close(f->f_file);
				f->f_un.f_pipe.f_pid = 0;
				log_deadchild(pid, status,
				    f->f_un.f_pipe.f_pname);
				break;
			}
		}
	}
}

/*
 * Return a printable representation of a host address (FQDN if available)
 */
const char *
cvthname(struct sockaddr_storage *f)
{
	int error;
	int niflag = NI_DGRAM;
	static char host[NI_MAXHOST], ip[NI_MAXHOST];

	error = getnameinfo((struct sockaddr*)f, ((struct sockaddr*)f)->sa_len,
	    ip, sizeof ip, NULL, 0, NI_NUMERICHOST|niflag);

	DPRINTF(D_CALL, "cvthname(%s)\n", ip);

	if (error) {
		DPRINTF(D_NET, "Malformed from address %s\n",
		    gai_strerror(error));
		return "???";
	}

	if (!UseNameService)
		return ip;

	error = getnameinfo((struct sockaddr*)f, ((struct sockaddr*)f)->sa_len,
	    host, sizeof host, NULL, 0, niflag);
	if (error) {
		DPRINTF(D_NET, "Host name for your address (%s) unknown\n", ip);
		return ip;
	}

	return host;
}

void
trim_anydomain(char *host)
{
	bool onlydigits = true;
	int i;

	if (!BSDOutputFormat)
		return;

	/* if non-digits found, then assume hostname and cut at first dot (this
	 * case also covers IPv6 addresses which should not contain dots),
	 * if only digits then assume IPv4 address and do not cut at all */
	for (i = 0; host[i]; i++) {
		if (host[i] == '.' && !onlydigits)
			host[i] = '\0';
		else if (!isdigit((unsigned char)host[i]) && host[i] != '.')
			onlydigits = false;
	}
}

static void
/*ARGSUSED*/
domark(int fd, short event, void *ev)
{
	struct event *ev_pass = (struct event *)ev;
	struct filed *f;
	dq_t q, nextq;
	sigset_t newmask, omask;

	schedule_event(&ev_pass,
		&((struct timeval){TIMERINTVL, 0}),
		domark, ev_pass);
	DPRINTF((D_CALL|D_EVENT), "domark()\n");

	BLOCK_SIGNALS(omask, newmask);
	now = time(NULL);
	MarkSeq += TIMERINTVL;
	if (MarkSeq >= MarkInterval) {
		logmsg_async(LOG_INFO, NULL, "-- MARK --", ADDDATE|MARK);
		MarkSeq = 0;
	}

	for (f = Files; f; f = f->f_next) {
		if (f->f_prevcount && now >= REPEATTIME(f)) {
			DPRINTF(D_DATA, "Flush %s: repeated %d times, %d sec.\n",
			    TypeInfo[f->f_type].name, f->f_prevcount,
			    repeatinterval[f->f_repeatcount]);
			fprintlog(f, NULL, NULL);
			BACKOFF(f);
		}
	}
	message_allqueues_check();
	RESTORE_SIGNALS(omask);

	/* Walk the dead queue, and see if we should signal somebody. */
	for (q = TAILQ_FIRST(&deadq_head); q != NULL; q = nextq) {
		nextq = TAILQ_NEXT(q, dq_entries);
		switch (q->dq_timeout) {
		case 0:
			/* Already signalled once, try harder now. */
			if (kill(q->dq_pid, SIGKILL) != 0)
				(void) deadq_remove(q->dq_pid);
			break;

		case 1:
			/*
			 * Timed out on the dead queue, send terminate
			 * signal.  Note that we leave the removal from
			 * the dead queue to reapchild(), which will
			 * also log the event (unless the process
			 * didn't even really exist, in case we simply
			 * drop it from the dead queue).
			 */
			if (kill(q->dq_pid, SIGTERM) != 0) {
				(void) deadq_remove(q->dq_pid);
				break;
			}
			/* FALLTHROUGH */

		default:
			q->dq_timeout--;
		}
	}
#ifndef DISABLE_SIGN
	if (GlobalSign.rsid) {	/* check if initialized */
		struct signature_group_t *sg;
		STAILQ_FOREACH(sg, &GlobalSign.SigGroups, entries) {
			sign_send_certificate_block(sg);
		}
	}
#endif /* !DISABLE_SIGN */
}

/*
 * Print syslogd errors some place.
 */
void
logerror(const char *fmt, ...)
{
	static int logerror_running;
	va_list ap;
	char tmpbuf[BUFSIZ];
	char buf[BUFSIZ];
	char *outbuf;

	/* If there's an error while trying to log an error, give up. */
	if (logerror_running)
		return;
	logerror_running = 1;

	va_start(ap, fmt);
	(void)vsnprintf(tmpbuf, sizeof(tmpbuf), fmt, ap);
	va_end(ap);

	if (errno) {
		(void)snprintf(buf, sizeof(buf), "%s: %s",
		    tmpbuf, strerror(errno));
		outbuf = buf;
	} else {
		(void)snprintf(buf, sizeof(buf), "%s", tmpbuf);
		outbuf = tmpbuf;
	}

	if (daemonized)
		logmsg_async(LOG_SYSLOG|LOG_ERR, NULL, outbuf, ADDDATE);
	if (!daemonized && Debug)
		DPRINTF(D_MISC, "%s\n", outbuf);
	if (!daemonized && !Debug)
		printf("%s\n", outbuf);

	logerror_running = 0;
}

/*
 * Print syslogd info some place.
 */
void
loginfo(const char *fmt, ...)
{
	va_list ap;
	char buf[BUFSIZ];

	va_start(ap, fmt);
	(void)vsnprintf(buf, sizeof(buf), fmt, ap);
	va_end(ap);

	DPRINTF(D_MISC, "%s\n", buf);
	logmsg_async(LOG_SYSLOG|LOG_INFO, NULL, buf, ADDDATE);
}

#ifndef DISABLE_TLS
static inline void
free_incoming_tls_sockets(void)
{
	struct TLS_Incoming_Conn *tls_in;
	int i;

	/*
	 * close all listening and connected TLS sockets
	 */
	if (TLS_Listen_Set)
		for (i = 0; i < TLS_Listen_Set->fd; i++) {
			if (close(TLS_Listen_Set[i+1].fd) == -1)
				logerror("close() failed");
			DEL_EVENT(TLS_Listen_Set[i+1].ev);
			FREEPTR(TLS_Listen_Set[i+1].ev);
		}
	FREEPTR(TLS_Listen_Set);
	/* close/free incoming TLS connections */
	while (!SLIST_EMPTY(&TLS_Incoming_Head)) {
		tls_in = SLIST_FIRST(&TLS_Incoming_Head);
		SLIST_REMOVE_HEAD(&TLS_Incoming_Head, entries);
		FREEPTR(tls_in->inbuf);
		free_tls_conn(tls_in->tls_conn);
		free(tls_in);
	}
}
#endif /* !DISABLE_TLS */

void
/*ARGSUSED*/
die(int fd, short event, void *ev)
{
	struct filed *f, *next;
	char **p;
	sigset_t newmask, omask;
	int i;
	size_t j;

	ShuttingDown = 1;	/* Don't log SIGCHLDs. */
	/* prevent recursive signals */
	BLOCK_SIGNALS(omask, newmask);

	errno = 0;
	if (ev != NULL)
		logerror("Exiting on signal %d", fd);
	else
		logerror("Fatal error, exiting");

	/*
	 *  flush any pending output
	 */
	for (f = Files; f != NULL; f = f->f_next) {
		/* flush any pending output */
		if (f->f_prevcount)
			fprintlog(f, NULL, NULL);
		SEND_QUEUE(f);
	}

#ifndef DISABLE_TLS
	free_incoming_tls_sockets();
#endif /* !DISABLE_TLS */
#ifndef DISABLE_SIGN
	sign_global_free();
#endif /* !DISABLE_SIGN */

	/*
	 *  Close all open log files.
	 */
	for (f = Files; f != NULL; f = next) {
		message_queue_freeall(f);

		switch (f->f_type) {
		case F_FILE:
		case F_TTY:
		case F_CONSOLE:
		case F_FIFO:
			if (f->f_file >= 0)
				(void)close(f->f_file);
			break;
		case F_PIPE:
			if (f->f_un.f_pipe.f_pid > 0) {
				(void)close(f->f_file);
			}
			f->f_un.f_pipe.f_pid = 0;
			break;
		case F_FORW:
			if (f->f_un.f_forw.f_addr)
				freeaddrinfo(f->f_un.f_forw.f_addr);
			break;
#ifndef DISABLE_TLS
		case F_TLS:
			free_tls_conn(f->f_un.f_tls.tls_conn);
			break;
#endif /* !DISABLE_TLS */
		}
		next = f->f_next;
		DELREF(f->f_prevmsg);
		FREEPTR(f->f_program);
		FREEPTR(f->f_host);
		DEL_EVENT(f->f_sq_event);
		free((char *)f);
	}

	/*
	 *  Close all open UDP sockets
	 */
	if (finet) {
		for (i = 0; i < finet->fd; i++) {
			if (close(finet[i+1].fd) < 0) {
				logerror("close() failed");
				die(0, 0, NULL);
			}
			DEL_EVENT(finet[i+1].ev);
			FREEPTR(finet[i+1].ev);
		}
		FREEPTR(finet);
	}

	/* free config options */
	for (j = 0; j < A_CNT(TypeInfo); j++) {
		FREEPTR(TypeInfo[j].queue_length_string);
		FREEPTR(TypeInfo[j].queue_size_string);
	}

#ifndef DISABLE_TLS
	FREEPTR(tls_opt.CAdir);
	FREEPTR(tls_opt.CAfile);
	FREEPTR(tls_opt.keyfile);
	FREEPTR(tls_opt.certfile);
	FREEPTR(tls_opt.x509verify);
	FREEPTR(tls_opt.bindhost);
	FREEPTR(tls_opt.bindport);
	FREEPTR(tls_opt.server);
	FREEPTR(tls_opt.gen_cert);
	free_cred_SLIST(&tls_opt.cert_head);
	free_cred_SLIST(&tls_opt.fprint_head);
	FREE_SSL_CTX(tls_opt.global_TLS_CTX);
#endif /* !DISABLE_TLS */

	FREEPTR(funix);
	for (p = LogPaths; p && *p; p++)
		unlink(*p);
	exit(0);
}

#ifndef DISABLE_SIGN
/*
 * get one "sign_delim_sg2" item, convert and store in ordered queue
 */
void
store_sign_delim_sg2(char *tmp_buf)
{
	struct string_queue *sqentry, *sqe1, *sqe2;

	if(!(sqentry = malloc(sizeof(*sqentry)))) {
		logerror("Unable to allocate memory");
		return;
	}
	/*LINTED constcond/null effect */
	assert(sizeof(int64_t) == sizeof(uint_fast64_t));
	if (dehumanize_number(tmp_buf, (int64_t*) &(sqentry->key)) == -1
	    || sqentry->key > (LOG_NFACILITIES<<3)) {
		DPRINTF(D_PARSE, "invalid sign_delim_sg2: %s\n", tmp_buf);
		free(sqentry);
		FREEPTR(tmp_buf);
		return;
	}
	sqentry->data = tmp_buf;

	if (STAILQ_EMPTY(&GlobalSign.sig2_delims)) {
		STAILQ_INSERT_HEAD(&GlobalSign.sig2_delims,
		    sqentry, entries);
		return;
	}

	/* keep delimiters sorted */
	sqe1 = sqe2 = STAILQ_FIRST(&GlobalSign.sig2_delims);
	if (sqe1->key > sqentry->key) {
		STAILQ_INSERT_HEAD(&GlobalSign.sig2_delims,
		    sqentry, entries);
		return;
	}

	while ((sqe1 = sqe2)
	   && (sqe2 = STAILQ_NEXT(sqe1, entries))) {
		if (sqe2->key > sqentry->key) {
			break;
		} else if (sqe2->key == sqentry->key) {
			DPRINTF(D_PARSE, "duplicate sign_delim_sg2: %s\n",
			    tmp_buf);
			FREEPTR(sqentry);
			FREEPTR(tmp_buf);
			return;
		}
	}
	STAILQ_INSERT_AFTER(&GlobalSign.sig2_delims, sqe1, sqentry, entries);
}
#endif /* !DISABLE_SIGN */

/*
 * read syslog.conf
 */
void
read_config_file(FILE *cf, struct filed **f_ptr)
{
	size_t linenum = 0;
	size_t i;
	struct filed *f, **nextp;
	char cline[LINE_MAX];
	char prog[NAME_MAX + 1];
	char host[MAXHOSTNAMELEN];
	const char *p;
	char *q;
	bool found_keyword;
#ifndef DISABLE_TLS
	struct peer_cred *cred = NULL;
	struct peer_cred_head *credhead = NULL;
#endif /* !DISABLE_TLS */
#ifndef DISABLE_SIGN
	char *sign_sg_str = NULL;
#endif /* !DISABLE_SIGN */
#if (!defined(DISABLE_TLS) || !defined(DISABLE_SIGN))
	char *tmp_buf = NULL;
#endif /* (!defined(DISABLE_TLS) || !defined(DISABLE_SIGN)) */
	/* central list of recognized configuration keywords
	 * and an address for their values as strings */
	const struct config_keywords {
		const char *keyword;
		char **variable;
	} config_keywords[] = {
#ifndef DISABLE_TLS
		/* TLS settings */
		{"tls_ca",		  &tls_opt.CAfile},
		{"tls_cadir",		  &tls_opt.CAdir},
		{"tls_cert",		  &tls_opt.certfile},
		{"tls_key",		  &tls_opt.keyfile},
		{"tls_verify",		  &tls_opt.x509verify},
		{"tls_bindport",	  &tls_opt.bindport},
		{"tls_bindhost",	  &tls_opt.bindhost},
		{"tls_server",		  &tls_opt.server},
		{"tls_gen_cert",	  &tls_opt.gen_cert},
		/* special cases in parsing */
		{"tls_allow_fingerprints",&tmp_buf},
		{"tls_allow_clientcerts", &tmp_buf},
		/* buffer settings */
		{"tls_queue_length",	  &TypeInfo[F_TLS].queue_length_string},
		{"tls_queue_size",	  &TypeInfo[F_TLS].queue_size_string},
#endif /* !DISABLE_TLS */
		{"file_queue_length",	  &TypeInfo[F_FILE].queue_length_string},
		{"pipe_queue_length",	  &TypeInfo[F_PIPE].queue_length_string},
		{"fifo_queue_length",	  &TypeInfo[F_FIFO].queue_length_string},
		{"file_queue_size",	  &TypeInfo[F_FILE].queue_size_string},
		{"pipe_queue_size",	  &TypeInfo[F_PIPE].queue_size_string},
		{"fifo_queue_size",	  &TypeInfo[F_FIFO].queue_size_string},
#ifndef DISABLE_SIGN
		/* syslog-sign setting */
		{"sign_sg",		  &sign_sg_str},
		/* also special case in parsing */
		{"sign_delim_sg2",	  &tmp_buf},
#endif /* !DISABLE_SIGN */
	};

	DPRINTF(D_CALL, "read_config_file()\n");

	/* free all previous config options */
	for (i = 0; i < A_CNT(TypeInfo); i++) {
		if (TypeInfo[i].queue_length_string
		    && TypeInfo[i].queue_length_string
		    != TypeInfo[i].default_length_string) {
			FREEPTR(TypeInfo[i].queue_length_string);
			TypeInfo[i].queue_length_string =
				strdup(TypeInfo[i].default_length_string);
		 }
		if (TypeInfo[i].queue_size_string
		    && TypeInfo[i].queue_size_string
		    != TypeInfo[i].default_size_string) {
			FREEPTR(TypeInfo[i].queue_size_string);
			TypeInfo[i].queue_size_string =
				strdup(TypeInfo[i].default_size_string);
		 }
	}
	for (i = 0; i < A_CNT(config_keywords); i++)
		FREEPTR(*config_keywords[i].variable);
	/*
	 * global settings
	 */
	while (fgets(cline, sizeof(cline), cf) != NULL) {
		linenum++;
		for (p = cline; isspace((unsigned char)*p); ++p)
			continue;
		if ((*p == '\0') || (*p == '#'))
			continue;

		for (i = 0; i < A_CNT(config_keywords); i++) {
			if (copy_config_value(config_keywords[i].keyword,
			    config_keywords[i].variable, &p, ConfFile,
			    linenum)) {
				DPRINTF((D_PARSE|D_MEM),
				    "found option %s, saved @%p\n",
				    config_keywords[i].keyword,
				    *config_keywords[i].variable);
#ifndef DISABLE_SIGN
				if (!strcmp("sign_delim_sg2",
				    config_keywords[i].keyword))
					do {
						store_sign_delim_sg2(tmp_buf);
					} while (copy_config_value_word(
					    &tmp_buf, &p));

#endif /* !DISABLE_SIGN */

#ifndef DISABLE_TLS
				/* special cases with multiple parameters */
				if (!strcmp("tls_allow_fingerprints",
				    config_keywords[i].keyword))
					credhead = &tls_opt.fprint_head;
				else if (!strcmp("tls_allow_clientcerts",
				    config_keywords[i].keyword))
					credhead = &tls_opt.cert_head;

				if (credhead) do {
					if(!(cred = malloc(sizeof(*cred)))) {
						logerror("Unable to "
							"allocate memory");
						break;
					}
					cred->data = tmp_buf;
					tmp_buf = NULL;
					SLIST_INSERT_HEAD(credhead,
						cred, entries);
				} while /* additional values? */
					(copy_config_value_word(&tmp_buf, &p));
				credhead = NULL;
				break;
#endif /* !DISABLE_TLS */
			}
		}
	}
	/* convert strings to integer values */
	for (i = 0; i < A_CNT(TypeInfo); i++) {
		if (!TypeInfo[i].queue_length_string
		    || dehumanize_number(TypeInfo[i].queue_length_string,
		    &TypeInfo[i].queue_length) == -1)
			if (dehumanize_number(TypeInfo[i].default_length_string,
			    &TypeInfo[i].queue_length) == -1)
				abort();
		if (!TypeInfo[i].queue_size_string
		    || dehumanize_number(TypeInfo[i].queue_size_string,
		    &TypeInfo[i].queue_size) == -1)
			if (dehumanize_number(TypeInfo[i].default_size_string,
			    &TypeInfo[i].queue_size) == -1)
				abort();
	}

#ifndef DISABLE_SIGN
	if (sign_sg_str) {
		if (sign_sg_str[1] == '\0'
		    && (sign_sg_str[0] == '0' || sign_sg_str[0] == '1'
		    || sign_sg_str[0] == '2' || sign_sg_str[0] == '3'))
			GlobalSign.sg = sign_sg_str[0] - '0';
		else {
			GlobalSign.sg = SIGN_SG;
			DPRINTF(D_MISC, "Invalid sign_sg value `%s', "
			    "use default value `%d'\n",
			    sign_sg_str, GlobalSign.sg);
		}
	} else	/* disable syslog-sign */
		GlobalSign.sg = -1;
#endif /* !DISABLE_SIGN */

	rewind(cf);
	linenum = 0;
	/*
	 *  Foreach line in the conf table, open that file.
	 */
	f = NULL;
	nextp = &f;

	strcpy(prog, "*");
	strcpy(host, "*");
	while (fgets(cline, sizeof(cline), cf) != NULL) {
		linenum++;
		found_keyword = false;
		/*
		 * check for end-of-section, comments, strip off trailing
		 * spaces and newline character.  #!prog is treated specially:
		 * following lines apply only to that program.
		 */
		for (p = cline; isspace((unsigned char)*p); ++p)
			continue;
		if (*p == '\0')
			continue;
		if (*p == '#') {
			p++;
			if (*p != '!' && *p != '+' && *p != '-')
				continue;
		}

		for (i = 0; i < A_CNT(config_keywords); i++) {
			if (!strncasecmp(p, config_keywords[i].keyword,
				strlen(config_keywords[i].keyword))) {
				DPRINTF(D_PARSE,
				    "skip cline %zu with keyword %s\n",
				    linenum, config_keywords[i].keyword);
				found_keyword = true;
			}
		}
		if (found_keyword)
			continue;

		if (*p == '+' || *p == '-') {
			host[0] = *p++;
			while (isspace((unsigned char)*p))
				p++;
			if (*p == '\0' || *p == '*') {
				strcpy(host, "*");
				continue;
			}
			/* the +hostname expression will continue
			 * to use the LocalHostName, not the FQDN */
			for (i = 1; i < MAXHOSTNAMELEN - 1; i++) {
				if (*p == '@') {
					(void)strncpy(&host[i], LocalHostName,
					    sizeof(host) - 1 - i);
					host[sizeof(host) - 1] = '\0';
					i = strlen(host) - 1;
					p++;
					continue;
				}
				if (!isalnum((unsigned char)*p) &&
				    *p != '.' && *p != '-' && *p != ',')
					break;
				host[i] = *p++;
			}
			host[i] = '\0';
			continue;
		}
		if (*p == '!') {
			p++;
			while (isspace((unsigned char)*p))
				p++;
			if (*p == '\0' || *p == '*') {
				strcpy(prog, "*");
				continue;
			}
			for (i = 0; i < NAME_MAX; i++) {
				if (!isprint((unsigned char)p[i]))
					break;
				prog[i] = p[i];
			}
			prog[i] = '\0';
			continue;
		}
		for (q = strchr(cline, '\0'); isspace((unsigned char)*--q);)
			continue;
		*++q = '\0';
		if ((f = calloc(1, sizeof(*f))) == NULL) {
			logerror("alloc failed");
			die(0, 0, NULL);
		}
		if (!*f_ptr) *f_ptr = f; /* return first node */
		*nextp = f;
		nextp = &f->f_next;
		cfline(linenum, cline, f, prog, host);
	}
}

/*
 *  INIT -- Initialize syslogd from configuration table
 */
void
/*ARGSUSED*/
init(int fd, short event, void *ev)
{
	FILE *cf;
	int i;
	struct filed *f, *newf, **nextp, *f2;
	char *p;
	sigset_t newmask, omask;
#ifndef DISABLE_TLS
	char *tls_status_msg = NULL;
	struct peer_cred *cred = NULL;
#endif /* !DISABLE_TLS */

	/* prevent recursive signals */
	BLOCK_SIGNALS(omask, newmask);

	DPRINTF((D_EVENT|D_CALL), "init\n");

	/*
	 * be careful about dependencies and order of actions:
	 * 1. flush buffer queues
	 * 2. flush -sign SBs
	 * 3. flush/delete buffer queue again, in case an SB got there
	 * 4. close files/connections
	 */

	/*
	 *  flush any pending output
	 */
	for (f = Files; f != NULL; f = f->f_next) {
		/* flush any pending output */
		if (f->f_prevcount)
			fprintlog(f, NULL, NULL);
		SEND_QUEUE(f);
	}
	/* some actions only on SIGHUP and not on first start */
	if (Initialized) {
#ifndef DISABLE_SIGN
		sign_global_free();
#endif /* !DISABLE_SIGN */
#ifndef DISABLE_TLS
		free_incoming_tls_sockets();
#endif /* !DISABLE_TLS */
		Initialized = 0;
	}
	/*
	 *  Close all open log files.
	 */
	for (f = Files; f != NULL; f = f->f_next) {
		switch (f->f_type) {
		case F_FILE:
		case F_TTY:
		case F_CONSOLE:
			(void)close(f->f_file);
			break;
		case F_PIPE:
			if (f->f_un.f_pipe.f_pid > 0) {
				(void)close(f->f_file);
				deadq_enter(f->f_un.f_pipe.f_pid,
				    f->f_un.f_pipe.f_pname);
			}
			f->f_un.f_pipe.f_pid = 0;
			break;
		case F_FORW:
			if (f->f_un.f_forw.f_addr)
				freeaddrinfo(f->f_un.f_forw.f_addr);
			break;
#ifndef DISABLE_TLS
		case F_TLS:
			free_tls_sslptr(f->f_un.f_tls.tls_conn);
			break;
#endif /* !DISABLE_TLS */
		}
	}

	/*
	 *  Close all open UDP sockets
	 */
	if (finet) {
		for (i = 0; i < finet->fd; i++) {
			if (close(finet[i+1].fd) < 0) {
				logerror("close() failed");
				die(0, 0, NULL);
			}
			DEL_EVENT(finet[i+1].ev);
			FREEPTR(finet[i+1].ev);
		}
		FREEPTR(finet);
	}

	/* get FQDN and hostname/domain */
	FREEPTR(oldLocalFQDN);
	oldLocalFQDN = LocalFQDN;
	LocalFQDN = getLocalFQDN();
	if ((p = strchr(LocalFQDN, '.')) != NULL)
		(void)strlcpy(LocalHostName, LocalFQDN, 1+p-LocalFQDN);
	else
		(void)strlcpy(LocalHostName, LocalFQDN, sizeof(LocalHostName));

	/*
	 *  Reset counter of forwarding actions
	 */

	NumForwards=0;

	/* new destination list to replace Files */
	newf = NULL;
	nextp = &newf;

	/* open the configuration file */
	if ((cf = fopen(ConfFile, "r")) == NULL) {
		DPRINTF(D_FILE, "Cannot open `%s'\n", ConfFile);
		*nextp = (struct filed *)calloc(1, sizeof(*f));
		cfline(0, "*.ERR\t/dev/console", *nextp, "*", "*");
		(*nextp)->f_next = (struct filed *)calloc(1, sizeof(*f));
		cfline(0, "*.PANIC\t*", (*nextp)->f_next, "*", "*");
		Initialized = 1;
		RESTORE_SIGNALS(omask);
		return;
	}

#ifndef DISABLE_TLS
	/* init with new TLS_CTX
	 * as far as I see one cannot change the cert/key of an existing CTX
	 */
	FREE_SSL_CTX(tls_opt.global_TLS_CTX);

	free_cred_SLIST(&tls_opt.cert_head);
	free_cred_SLIST(&tls_opt.fprint_head);
#endif /* !DISABLE_TLS */

	/* read and close configuration file */
	read_config_file(cf, &newf);
	newf = *nextp;
	(void)fclose(cf);
	DPRINTF(D_MISC, "read_config_file() returned newf=%p\n", newf);

#define MOVE_QUEUE(dst, src) do {				\
	struct buf_queue *buf;					\
	STAILQ_CONCAT(&dst->f_qhead, &src->f_qhead);		\
	STAILQ_FOREACH(buf, &dst->f_qhead, entries) {		\
	      dst->f_qelements++;				\
	      dst->f_qsize += buf_queue_obj_size(buf);		\
	}							\
	src->f_qsize = 0;					\
	src->f_qelements = 0;					\
} while (/*CONSTCOND*/0)

	/*
	 *  Free old log files.
	 */
	for (f = Files; f != NULL;) {
		struct filed *ftmp;

		/* check if a new logfile is equal, if so pass the queue */
		for (f2 = newf; f2 != NULL; f2 = f2->f_next) {
			if (f->f_type == f2->f_type
			    && ((f->f_type == F_PIPE
			    && !strcmp(f->f_un.f_pipe.f_pname,
			    f2->f_un.f_pipe.f_pname))
#ifndef DISABLE_TLS
			    || (f->f_type == F_TLS
			    && !strcmp(f->f_un.f_tls.tls_conn->hostname,
			    f2->f_un.f_tls.tls_conn->hostname)
			    && !strcmp(f->f_un.f_tls.tls_conn->port,
			    f2->f_un.f_tls.tls_conn->port))
#endif /* !DISABLE_TLS */
			    || (f->f_type == F_FORW
			    && !strcmp(f->f_un.f_forw.f_hname,
			    f2->f_un.f_forw.f_hname)))) {
				DPRINTF(D_BUFFER, "move queue from f@%p "
				    "to f2@%p\n", f, f2);
				MOVE_QUEUE(f2, f);
			 }
		}
		message_queue_freeall(f);
		DELREF(f->f_prevmsg);
#ifndef DISABLE_TLS
		if (f->f_type == F_TLS)
			free_tls_conn(f->f_un.f_tls.tls_conn);
#endif /* !DISABLE_TLS */
		FREEPTR(f->f_program);
		FREEPTR(f->f_host);
		DEL_EVENT(f->f_sq_event);

		ftmp = f->f_next;
		free((char *)f);
		f = ftmp;
	}
	Files = newf;
	Initialized = 1;

	if (Debug) {
		for (f = Files; f; f = f->f_next) {
			for (i = 0; i <= LOG_NFACILITIES; i++)
				if (f->f_pmask[i] == INTERNAL_NOPRI)
					printf("X ");
				else
					printf("%d ", f->f_pmask[i]);
			printf("%s: ", TypeInfo[f->f_type].name);
			switch (f->f_type) {
			case F_FILE:
			case F_TTY:
			case F_CONSOLE:
			case F_FIFO:
				printf("%s", f->f_un.f_fname);
				break;

			case F_FORW:
				printf("%s", f->f_un.f_forw.f_hname);
				break;
#ifndef DISABLE_TLS
			case F_TLS:
				printf("[%s]", f->f_un.f_tls.tls_conn->hostname);
				break;
#endif /* !DISABLE_TLS */
			case F_PIPE:
				printf("%s", f->f_un.f_pipe.f_pname);
				break;

			case F_USERS:
				for (i = 0;
				    i < MAXUNAMES && *f->f_un.f_uname[i]; i++)
					printf("%s, ", f->f_un.f_uname[i]);
				break;
			}
			if (f->f_program != NULL)
				printf(" (%s)", f->f_program);
			printf("\n");
		}
	}

	finet = socksetup(PF_UNSPEC, bindhostname);
	if (finet) {
		if (SecureMode) {
			for (i = 0; i < finet->fd; i++) {
				if (shutdown(finet[i+1].fd, SHUT_RD) < 0) {
					logerror("shutdown() failed");
					die(0, 0, NULL);
				}
			}
		} else
			DPRINTF(D_NET, "Listening on inet and/or inet6 socket\n");
		DPRINTF(D_NET, "Sending on inet and/or inet6 socket\n");
	}

#ifndef DISABLE_TLS
	/* TLS setup -- after all local destinations opened  */
	DPRINTF(D_PARSE, "Parsed options: tls_ca: %s, tls_cadir: %s, "
	    "tls_cert: %s, tls_key: %s, tls_verify: %s, "
	    "bind: %s:%s, max. queue_lengths: %"
	    PRId64 ", %" PRId64 ", %" PRId64 ", "
	    "max. queue_sizes: %"
	    PRId64 ", %" PRId64 ", %" PRId64 "\n",
	    tls_opt.CAfile, tls_opt.CAdir,
	    tls_opt.certfile, tls_opt.keyfile, tls_opt.x509verify,
	    tls_opt.bindhost, tls_opt.bindport,
	    TypeInfo[F_TLS].queue_length, TypeInfo[F_FILE].queue_length,
	    TypeInfo[F_PIPE].queue_length,
	    TypeInfo[F_TLS].queue_size, TypeInfo[F_FILE].queue_size,
	    TypeInfo[F_PIPE].queue_size);
	SLIST_FOREACH(cred, &tls_opt.cert_head, entries) {
		DPRINTF(D_PARSE, "Accepting peer certificate "
		    "from file: \"%s\"\n", cred->data);
	}
	SLIST_FOREACH(cred, &tls_opt.fprint_head, entries) {
		DPRINTF(D_PARSE, "Accepting peer certificate with "
		    "fingerprint: \"%s\"\n", cred->data);
	}

	/* Note: The order of initialization is important because syslog-sign
	 * should use the TLS cert for signing. -- So we check first if TLS
	 * will be used and initialize it before starting -sign.
	 *
	 * This means that if we are a client without TLS destinations TLS
	 * will not be initialized and syslog-sign will generate a new key.
	 * -- Even if the user has set a usable tls_cert.
	 * Is this the expected behaviour? The alternative would be to always
	 * initialize the TLS structures, even if they will not be needed
	 * (or only needed to read the DSA key for -sign).
	 */

	/* Initialize TLS only if used */
	if (tls_opt.server)
		tls_status_msg = init_global_TLS_CTX();
	else
		for (f = Files; f; f = f->f_next) {
			if (f->f_type != F_TLS)
				continue;
			tls_status_msg = init_global_TLS_CTX();
			break;
		}

#endif /* !DISABLE_TLS */

#ifndef DISABLE_SIGN
	/* only initialize -sign if actually used */
	if (GlobalSign.sg == 0 || GlobalSign.sg == 1 || GlobalSign.sg == 2)
		(void)sign_global_init(Files);
	else if (GlobalSign.sg == 3)
		for (f = Files; f; f = f->f_next)
			if (f->f_flags & FFLAG_SIGN) {
				(void)sign_global_init(Files);
				break;
			}
#endif /* !DISABLE_SIGN */

#ifndef DISABLE_TLS
	if (tls_status_msg) {
		loginfo("%s", tls_status_msg);
		free(tls_status_msg);
	}
	DPRINTF((D_NET|D_TLS), "Preparing sockets for TLS\n");
	TLS_Listen_Set =
		socksetup_tls(PF_UNSPEC, tls_opt.bindhost, tls_opt.bindport);

	for (f = Files; f; f = f->f_next) {
		if (f->f_type != F_TLS)
			continue;
		if (!tls_connect(f->f_un.f_tls.tls_conn)) {
			logerror("Unable to connect to TLS server %s",
			    f->f_un.f_tls.tls_conn->hostname);
			/* Reconnect after x seconds  */
			schedule_event(&f->f_un.f_tls.tls_conn->event,
			    &((struct timeval){TLS_RECONNECT_SEC, 0}),
			    tls_reconnect, f->f_un.f_tls.tls_conn);
		}
	}
#endif /* !DISABLE_TLS */

	loginfo("restart");
	/*
	 * Log a change in hostname, but only on a restart (we detect this
	 * by checking to see if we're passed a kevent).
	 */
	if (oldLocalFQDN && strcmp(oldLocalFQDN, LocalFQDN) != 0)
		loginfo("host name changed, \"%s\" to \"%s\"",
		    oldLocalFQDN, LocalFQDN);

	RESTORE_SIGNALS(omask);
}

/*
 * Crack a configuration file line
 */
void
cfline(size_t linenum, const char *line, struct filed *f, const char *prog,
    const char *host)
{
	struct addrinfo hints, *res;
	int    error, i, pri, syncfile;
	const char   *p, *q;
	char *bp;
	char   buf[MAXLINE];
	struct stat sb;

	DPRINTF((D_CALL|D_PARSE),
		"cfline(%zu, \"%s\", f, \"%s\", \"%s\")\n",
		linenum, line, prog, host);

	errno = 0;	/* keep strerror() stuff out of logerror messages */

	/* clear out file entry */
	memset(f, 0, sizeof(*f));
	for (i = 0; i <= LOG_NFACILITIES; i++)
		f->f_pmask[i] = INTERNAL_NOPRI;
	STAILQ_INIT(&f->f_qhead);

	/*
	 * There should not be any space before the log facility.
	 * Check this is okay, complain and fix if it is not.
	 */
	q = line;
	if (isblank((unsigned char)*line)) {
		errno = 0;
		logerror("Warning: `%s' space or tab before the log facility",
		    line);
		/* Fix: strip all spaces/tabs before the log facility */
		while (*q++ && isblank((unsigned char)*q))
			/* skip blanks */;
		line = q;
	}

	/*
	 * q is now at the first char of the log facility
	 * There should be at least one tab after the log facility
	 * Check this is okay, and complain and fix if it is not.
	 */
	q = line + strlen(line);
	while (!isblank((unsigned char)*q) && (q != line))
		q--;
	if ((q == line) && strlen(line)) {
		/* No tabs or space in a non empty line: complain */
		errno = 0;
		logerror(
		    "Error: `%s' log facility or log target missing",
		    line);
		return;
	}

	/* save host name, if any */
	if (*host == '*')
		f->f_host = NULL;
	else {
		f->f_host = strdup(host);
		trim_anydomain(&f->f_host[1]);	/* skip +/- at beginning */
	}

	/* save program name, if any */
	if (*prog == '*')
		f->f_program = NULL;
	else
		f->f_program = strdup(prog);

	/* scan through the list of selectors */
	for (p = line; *p && !isblank((unsigned char)*p);) {
		int pri_done, pri_cmp, pri_invert;

		/* find the end of this facility name list */
		for (q = p; *q && !isblank((unsigned char)*q) && *q++ != '.'; )
			continue;

		/* get the priority comparison */
		pri_cmp = 0;
		pri_done = 0;
		pri_invert = 0;
		if (*q == '!') {
			pri_invert = 1;
			q++;
		}
		while (! pri_done) {
			switch (*q) {
			case '<':
				pri_cmp = PRI_LT;
				q++;
				break;
			case '=':
				pri_cmp = PRI_EQ;
				q++;
				break;
			case '>':
				pri_cmp = PRI_GT;
				q++;
				break;
			default:
				pri_done = 1;
				break;
			}
		}

		/* collect priority name */
		for (bp = buf; *q && !strchr("\t ,;", *q); )
			*bp++ = *q++;
		*bp = '\0';

		/* skip cruft */
		while (strchr(",;", *q))
			q++;

		/* decode priority name */
		if (*buf == '*') {
			pri = LOG_PRIMASK + 1;
			pri_cmp = PRI_LT | PRI_EQ | PRI_GT;
		} else {
			pri = decode(buf, prioritynames);
			if (pri < 0) {
				errno = 0;
				logerror("Unknown priority name `%s'", buf);
				return;
			}
		}
		if (pri_cmp == 0)
			pri_cmp = UniquePriority ? PRI_EQ
						 : PRI_EQ | PRI_GT;
		if (pri_invert)
			pri_cmp ^= PRI_LT | PRI_EQ | PRI_GT;

		/* scan facilities */
		while (*p && !strchr("\t .;", *p)) {
			for (bp = buf; *p && !strchr("\t ,;.", *p); )
				*bp++ = *p++;
			*bp = '\0';
			if (*buf == '*')
				for (i = 0; i < LOG_NFACILITIES; i++) {
					f->f_pmask[i] = pri;
					f->f_pcmp[i] = pri_cmp;
				}
			else {
				i = decode(buf, facilitynames);
				if (i < 0) {
					errno = 0;
					logerror("Unknown facility name `%s'",
					    buf);
					return;
				}
				f->f_pmask[i >> 3] = pri;
				f->f_pcmp[i >> 3] = pri_cmp;
			}
			while (*p == ',' || *p == ' ')
				p++;
		}

		p = q;
	}

	/* skip to action part */
	while (isblank((unsigned char)*p))
		p++;

	/*
	 * should this be "#ifndef DISABLE_SIGN" or is it a general option?
	 * '+' before file destination: write with PRI field for later
	 * verification
	 */
	if (*p == '+') {
		f->f_flags |= FFLAG_FULL;
		p++;
	}
	if (*p == '-') {
		syncfile = 0;
		p++;
	} else
		syncfile = 1;

	switch (*p) {
	case '@':
#ifndef DISABLE_SIGN
		if (GlobalSign.sg == 3)
			f->f_flags |= FFLAG_SIGN;
#endif /* !DISABLE_SIGN */
#ifndef DISABLE_TLS
		if (*(p+1) == '[') {
			/* TLS destination */
			if (!parse_tls_destination(p, f, linenum)) {
				logerror("Unable to parse action %s", p);
				break;
			}
			f->f_type = F_TLS;
			break;
		}
#endif /* !DISABLE_TLS */
		(void)strlcpy(f->f_un.f_forw.f_hname, ++p,
		    sizeof(f->f_un.f_forw.f_hname));
		memset(&hints, 0, sizeof(hints));
		hints.ai_family = AF_UNSPEC;
		hints.ai_socktype = SOCK_DGRAM;
		hints.ai_protocol = 0;
		error = getaddrinfo(f->f_un.f_forw.f_hname, "syslog", &hints,
		    &res);
		if (error) {
			errno = 0;
			logerror("%s", gai_strerror(error));
			break;
		}
		f->f_un.f_forw.f_addr = res;
		f->f_type = F_FORW;
		NumForwards++;
		break;

	case '/':
#ifndef DISABLE_SIGN
		if (GlobalSign.sg == 3)
			f->f_flags |= FFLAG_SIGN;
#endif /* !DISABLE_SIGN */
		(void)strlcpy(f->f_un.f_fname, p, sizeof(f->f_un.f_fname));
		if ((f->f_file = open(p, O_WRONLY|O_APPEND|O_NONBLOCK, 0)) < 0)
		{
			f->f_type = F_UNUSED;
			logerror("%s", p);
			break;
		}
		if (!fstat(f->f_file, &sb) && S_ISFIFO(sb.st_mode)) {
			f->f_file = -1;
			f->f_type = F_FIFO;
			break;
		}

		if (isatty(f->f_file)) {
			f->f_type = F_TTY;
			if (strcmp(p, ctty) == 0)
				f->f_type = F_CONSOLE;
		} else
			f->f_type = F_FILE;

		if (syncfile)
			f->f_flags |= FFLAG_SYNC;
		break;

	case '|':
#ifndef DISABLE_SIGN
		if (GlobalSign.sg == 3)
			f->f_flags |= FFLAG_SIGN;
#endif
		f->f_un.f_pipe.f_pid = 0;
		(void) strlcpy(f->f_un.f_pipe.f_pname, p + 1,
		    sizeof(f->f_un.f_pipe.f_pname));
		f->f_type = F_PIPE;
		break;

	case '*':
		f->f_type = F_WALL;
		break;

	default:
		for (i = 0; i < MAXUNAMES && *p; i++) {
			for (q = p; *q && *q != ','; )
				q++;
			(void)strncpy(f->f_un.f_uname[i], p, UT_NAMESIZE);
			if ((q - p) > UT_NAMESIZE)
				f->f_un.f_uname[i][UT_NAMESIZE] = '\0';
			else
				f->f_un.f_uname[i][q - p] = '\0';
			while (*q == ',' || *q == ' ')
				q++;
			p = q;
		}
		f->f_type = F_USERS;
		break;
	}
}


/*
 *  Decode a symbolic name to a numeric value
 */
int
decode(const char *name, CODE *codetab)
{
	CODE *c;
	char *p, buf[40];

	if (isdigit((unsigned char)*name))
		return atoi(name);

	for (p = buf; *name && p < &buf[sizeof(buf) - 1]; p++, name++) {
		if (isupper((unsigned char)*name))
			*p = tolower((unsigned char)*name);
		else
			*p = *name;
	}
	*p = '\0';
	for (c = codetab; c->c_name; c++)
		if (!strcmp(buf, c->c_name))
			return c->c_val;

	return -1;
}

/*
 * Retrieve the size of the kernel message buffer, via sysctl.
 */
int
getmsgbufsize(void)
{
#ifdef __NetBSD_Version__
	int msgbufsize, mib[2];
	size_t size;

	mib[0] = CTL_KERN;
	mib[1] = KERN_MSGBUFSIZE;
	size = sizeof msgbufsize;
	if (sysctl(mib, 2, &msgbufsize, &size, NULL, 0) == -1) {
		DPRINTF(D_MISC, "Couldn't get kern.msgbufsize\n");
		return 0;
	}
	return msgbufsize;
#else
	return MAXLINE;
#endif /* __NetBSD_Version__ */
}

/*
 * Retrieve the hostname, via sysctl.
 */
char *
getLocalFQDN(void)
{
	int mib[2];
	char *hostname;
	size_t len;

	mib[0] = CTL_KERN;
	mib[1] = KERN_HOSTNAME;
	sysctl(mib, 2, NULL, &len, NULL, 0);

	if (!(hostname = malloc(len))) {
		logerror("Unable to allocate memory");
		die(0,0,NULL);
	} else if (sysctl(mib, 2, hostname, &len, NULL, 0) == -1) {
		DPRINTF(D_MISC, "Couldn't get kern.hostname\n");
		(void)gethostname(hostname, sizeof(len));
	}
	return hostname;
}

struct socketEvent *
socksetup(int af, const char *hostname)
{
	struct addrinfo hints, *res, *r;
	int error, maxs;
#ifdef IPV6_V6ONLY
	int on = 1;
#endif /* IPV6_V6ONLY */
	struct socketEvent *s, *socks;

	if(SecureMode && !NumForwards)
		return NULL;

	memset(&hints, 0, sizeof(hints));
	hints.ai_flags = AI_PASSIVE;
	hints.ai_family = af;
	hints.ai_socktype = SOCK_DGRAM;
	error = getaddrinfo(hostname, "syslog", &hints, &res);
	if (error) {
		errno = 0;
		logerror("%s", gai_strerror(error));
		die(0, 0, NULL);
	}

	/* Count max number of sockets we may open */
	for (maxs = 0, r = res; r; r = r->ai_next, maxs++)
		continue;
	socks = calloc(maxs+1, sizeof(*socks));
	if (!socks) {
		logerror("Couldn't allocate memory for sockets");
		die(0, 0, NULL);
	}

	socks->fd = 0;	 /* num of sockets counter at start of array */
	s = socks + 1;
	for (r = res; r; r = r->ai_next) {
		s->fd = socket(r->ai_family, r->ai_socktype, r->ai_protocol);
		if (s->fd < 0) {
			logerror("socket() failed");
			continue;
		}
		s->af = r->ai_family;
#ifdef IPV6_V6ONLY
		if (r->ai_family == AF_INET6 && setsockopt(s->fd, IPPROTO_IPV6,
		    IPV6_V6ONLY, &on, sizeof(on)) < 0) {
			logerror("setsockopt(IPV6_V6ONLY) failed");
			close(s->fd);
			continue;
		}
#endif /* IPV6_V6ONLY */

		if (!SecureMode) {
			if (bind(s->fd, r->ai_addr, r->ai_addrlen) < 0) {
				logerror("bind() failed");
				close(s->fd);
				continue;
			}
			s->ev = allocev();
			event_set(s->ev, s->fd, EV_READ | EV_PERSIST,
				dispatch_read_finet, s->ev);
			if (event_add(s->ev, NULL) == -1) {
				DPRINTF((D_EVENT|D_NET),
				    "Failure in event_add()\n");
			} else {
				DPRINTF((D_EVENT|D_NET),
				    "Listen on UDP port "
				    "(event@%p)\n", s->ev);
			}
		}

		socks->fd++;  /* num counter */
		s++;
	}

	if (res)
		freeaddrinfo(res);
	if (socks->fd == 0) {
		free (socks);
		if(Debug)
			return NULL;
		else
			die(0, 0, NULL);
	}
	return socks;
}

/*
 * Fairly similar to popen(3), but returns an open descriptor, as opposed
 * to a FILE *.
 */
int
p_open(char *prog, pid_t *rpid)
{
	static char sh[] = "sh", mc[] = "-c";
	int pfd[2], nulldesc, i;
	pid_t pid;
	char *argv[4];	/* sh -c cmd NULL */

	if (pipe(pfd) == -1)
		return -1;
	if ((nulldesc = open(_PATH_DEVNULL, O_RDWR)) == -1) {
		/* We are royally screwed anyway. */
		return -1;
	}

	switch ((pid = fork())) {
	case -1:
		(void) close(nulldesc);
		return -1;

	case 0:
		argv[0] = sh;
		argv[1] = mc;
		argv[2] = prog;
		argv[3] = NULL;

		(void) setsid();	/* avoid catching SIGHUPs. */

		/*
		 * Reset ignored signals to their default behavior.
		 */
		(void)signal(SIGTERM, SIG_DFL);
		(void)signal(SIGINT, SIG_DFL);
		(void)signal(SIGQUIT, SIG_DFL);
		(void)signal(SIGPIPE, SIG_DFL);
		(void)signal(SIGHUP, SIG_DFL);

		dup2(pfd[0], STDIN_FILENO);
		dup2(nulldesc, STDOUT_FILENO);
		dup2(nulldesc, STDERR_FILENO);
		for (i = getdtablesize(); i > 2; i--)
			(void) close(i);

		(void) execvp(_PATH_BSHELL, argv);
		_exit(255);
	}

	(void) close(nulldesc);
	(void) close(pfd[0]);

	/*
	 * Avoid blocking on a hung pipe.  With O_NONBLOCK, we are
	 * supposed to get an EWOULDBLOCK on writev(2), which is
	 * caught by the logic above anyway, which will in turn
	 * close the pipe, and fork a new logging subprocess if
	 * necessary.  The stale subprocess will be killed some
	 * time later unless it terminated itself due to closing
	 * its input pipe.
	 */
	if (fcntl(pfd[1], F_SETFL, O_NONBLOCK) == -1) {
		/* This is bad. */
		logerror("Warning: cannot change pipe to pid %d to "
		    "non-blocking.", (int) pid);
	}
	*rpid = pid;
	return pfd[1];
}

void
deadq_enter(pid_t pid, const char *name)
{
	dq_t p;
	int status;

	/*
	 * Be paranoid: if we can't signal the process, don't enter it
	 * into the dead queue (perhaps it's already dead).  If possible,
	 * we try to fetch and log the child's status.
	 */
	if (kill(pid, 0) != 0) {
		if (waitpid(pid, &status, WNOHANG) > 0)
			log_deadchild(pid, status, name);
		return;
	}

	p = malloc(sizeof(*p));
	if (p == NULL) {
		logerror("panic: out of memory!");
		exit(1);
	}

	p->dq_pid = pid;
	p->dq_timeout = DQ_TIMO_INIT;
	TAILQ_INSERT_TAIL(&deadq_head, p, dq_entries);
}

int
deadq_remove(pid_t pid)
{
	dq_t q;

	for (q = TAILQ_FIRST(&deadq_head); q != NULL;
	     q = TAILQ_NEXT(q, dq_entries)) {
		if (q->dq_pid == pid) {
			TAILQ_REMOVE(&deadq_head, q, dq_entries);
			free(q);
			return 1;
		}
	}
	return 0;
}

void
log_deadchild(pid_t pid, int status, const char *name)
{
	int code;
	char buf[256];
	const char *reason;

	/* Keep strerror() struff out of logerror messages. */
	errno = 0;
	if (WIFSIGNALED(status)) {
		reason = "due to signal";
		code = WTERMSIG(status);
	} else {
		reason = "with status";
		code = WEXITSTATUS(status);
		if (code == 0)
			return;
	}
	(void) snprintf(buf, sizeof(buf),
	    "Logging subprocess %d (%s) exited %s %d.",
	    pid, name, reason, code);
	logerror("%s", buf);
}

struct event *
allocev(void)
{
	struct event *ev;

	if (!(ev = calloc(1, sizeof(*ev))))
		logerror("Unable to allocate memory");
	return ev;
}

/* *ev is allocated if necessary */
void
schedule_event(struct event **ev, struct timeval *tv,
	void (*cb)(int, short, void *), void *arg)
{
	if (!*ev && !(*ev = allocev())) {
		return;
	}
	event_set(*ev, 0, 0, cb, arg);
	DPRINTF(D_EVENT, "event_add(%s@%p)\n", "schedule_ev", *ev); \
	if (event_add(*ev, tv) == -1) {
		DPRINTF(D_EVENT, "Failure in event_add()\n");
	}
}

#ifndef DISABLE_TLS
/* abbreviation for freeing credential lists */
void
free_cred_SLIST(struct peer_cred_head *head)
{
	struct peer_cred *cred;

	while (!SLIST_EMPTY(head)) {
		cred = SLIST_FIRST(head);
		SLIST_REMOVE_HEAD(head, entries);
		FREEPTR(cred->data);
		free(cred);
	}
}
#endif /* !DISABLE_TLS */

/*
 * send message queue after reconnect
 */
/*ARGSUSED*/
void
send_queue(int fd, short event, void *arg)
{
	struct filed *f = (struct filed *) arg;
	struct buf_queue *qentry;
#define SQ_CHUNK_SIZE 250
	size_t cnt = 0;

#ifndef DISABLE_TLS
	if (f->f_type == F_TLS) {
		/* use a flag to prevent recursive calls to send_queue() */
		if (f->f_un.f_tls.tls_conn->send_queue)
			return;
		else
			f->f_un.f_tls.tls_conn->send_queue = true;
	}
	DPRINTF((D_DATA|D_CALL), "send_queue(f@%p with %zu msgs, "
		"cnt@%p = %zu)\n", f, f->f_qelements, &cnt, cnt);
#endif /* !DISABLE_TLS */

	while ((qentry = STAILQ_FIRST(&f->f_qhead))) {
#ifndef DISABLE_TLS
		/* send_queue() might be called with an unconnected destination
		 * from init() or die() or one message might take longer,
		 * leaving the connection in state ST_WAITING and thus not
		 * ready for the next message.
		 * this check is a shortcut to skip these unnecessary calls */
		if (f->f_type == F_TLS
		    && f->f_un.f_tls.tls_conn->state != ST_TLS_EST) {
			DPRINTF(D_TLS, "abort send_queue(cnt@%p = %zu) "
			    "on TLS connection in state %d\n",
			    &cnt, cnt, f->f_un.f_tls.tls_conn->state);
			return;
		 }
#endif /* !DISABLE_TLS */
		fprintlog(f, qentry->msg, qentry);

		/* Sending a long queue can take some time during which
		 * SIGHUP and SIGALRM are blocked and no events are handled.
		 * To avoid that we only send SQ_CHUNK_SIZE messages at once
		 * and then reschedule ourselves to continue. Thus the control
		 * will return first from all signal-protected functions so a
		 * possible SIGHUP/SIGALRM is handled and then back to the
		 * main loop which can handle possible input.
		 */
		if (++cnt >= SQ_CHUNK_SIZE) {
			if (!f->f_sq_event) { /* alloc on demand */
				f->f_sq_event = allocev();
				event_set(f->f_sq_event, 0, 0, send_queue, f);
			}
			if (event_add(f->f_sq_event, &((struct timeval){0, 1})) == -1) {
				DPRINTF(D_EVENT, "Failure in event_add()\n");
			}
			break;
		}
	}
#ifndef DISABLE_TLS
	if (f->f_type == F_TLS)
		f->f_un.f_tls.tls_conn->send_queue = false;
#endif

}

/*
 * finds the next queue element to delete
 *
 * has stateful behaviour, before using it call once with reset = true
 * after that every call will return one next queue elemen to delete,
 * depending on strategy either the oldest or the one with the lowest priority
 */
static struct buf_queue *
find_qentry_to_delete(const struct buf_queue_head *head, int strategy,
    bool reset)
{
	static int pri;
	static struct buf_queue *qentry_static;

	struct buf_queue *qentry_tmp;

	if (reset || STAILQ_EMPTY(head)) {
		pri = LOG_DEBUG;
		qentry_static = STAILQ_FIRST(head);
		return NULL;
	}

	/* find elements to delete */
	if (strategy == PURGE_BY_PRIORITY) {
		qentry_tmp = qentry_static;
		if (!qentry_tmp) return NULL;
		while ((qentry_tmp = STAILQ_NEXT(qentry_tmp, entries)) != NULL)
		{
			if (LOG_PRI(qentry_tmp->msg->pri) == pri) {
				/* save the successor, because qentry_tmp
				 * is probably deleted by the caller */
				qentry_static = STAILQ_NEXT(qentry_tmp, entries);
				return qentry_tmp;
			}
		}
		/* nothing found in while loop --> next pri */
		if (--pri)
			return find_qentry_to_delete(head, strategy, false);
		else
			return NULL;
	} else /* strategy == PURGE_OLDEST or other value */ {
		qentry_tmp = qentry_static;
		qentry_static = STAILQ_NEXT(qentry_tmp, entries);
		return qentry_tmp;  /* is NULL on empty queue */
	}
}

/* note on TAILQ: newest message added at TAIL,
 *		  oldest to be removed is FIRST
 */
/*
 * checks length of a destination's message queue
 * if del_entries == 0 then assert queue length is
 *   less or equal to configured number of queue elements
 * otherwise del_entries tells how many entries to delete
 *
 * returns the number of removed queue elements
 * (which not necessarily means free'd messages)
 *
 * strategy PURGE_OLDEST to delete oldest entry, e.g. after it was resent
 * strategy PURGE_BY_PRIORITY to delete messages with lowest priority first,
 *	this is much slower but might be desirable when unsent messages have
 *	to be deleted, e.g. in call from domark()
 */
size_t
message_queue_purge(struct filed *f, size_t del_entries, int strategy)
{
	size_t removed = 0;
	struct buf_queue *qentry = NULL;

	DPRINTF((D_CALL|D_BUFFER), "purge_message_queue(%p, %zu, %d) with "
	    "f_qelements=%zu and f_qsize=%zu\n",
	    f, del_entries, strategy,
	    f->f_qelements, f->f_qsize);

	/* reset state */
	(void)find_qentry_to_delete(&f->f_qhead, strategy, true);

	while (removed < del_entries
	    || (TypeInfo[f->f_type].queue_length != -1
	    && (size_t)TypeInfo[f->f_type].queue_length <= f->f_qelements)
	    || (TypeInfo[f->f_type].queue_size != -1
	    && (size_t)TypeInfo[f->f_type].queue_size <= f->f_qsize)) {
		qentry = find_qentry_to_delete(&f->f_qhead, strategy, 0);
		if (message_queue_remove(f, qentry))
			removed++;
		else
			break;
	}
	return removed;
}

/* run message_queue_purge() for all destinations to free memory */
size_t
message_allqueues_purge(void)
{
	size_t sum = 0;
	struct filed *f;

	for (f = Files; f; f = f->f_next)
		sum += message_queue_purge(f,
		    f->f_qelements/10, PURGE_BY_PRIORITY);

	DPRINTF(D_BUFFER,
	    "message_allqueues_purge(): removed %zu buffer entries\n", sum);
	return sum;
}

/* run message_queue_purge() for all destinations to check limits */
size_t
message_allqueues_check(void)
{
	size_t sum = 0;
	struct filed *f;

	for (f = Files; f; f = f->f_next)
		sum += message_queue_purge(f, 0, PURGE_BY_PRIORITY);
	DPRINTF(D_BUFFER,
	    "message_allqueues_check(): removed %zu buffer entries\n", sum);
	return sum;
}

struct buf_msg *
buf_msg_new(const size_t len)
{
	struct buf_msg *newbuf;

	CALLOC(newbuf, sizeof(*newbuf));

	if (len) { /* len = 0 is valid */
		MALLOC(newbuf->msg, len);
		newbuf->msgorig = newbuf->msg;
		newbuf->msgsize = len;
	}
	return NEWREF(newbuf);
}

void
buf_msg_free(struct buf_msg *buf)
{
	if (!buf)
		return;

	buf->refcount--;
	if (buf->refcount == 0) {
		FREEPTR(buf->timestamp);
		/* small optimizations: the host/recvhost may point to the
		 * global HostName/FQDN. of course this must not be free()d
		 * same goes for appname and include_pid
		 */
		if (buf->recvhost != buf->host
		    && buf->recvhost != LocalHostName
		    && buf->recvhost != LocalFQDN
		    && buf->recvhost != oldLocalFQDN)
			FREEPTR(buf->recvhost);
		if (buf->host != LocalHostName
		    && buf->host != LocalFQDN
		    && buf->host != oldLocalFQDN)
			FREEPTR(buf->host);
		if (buf->prog != appname)
			FREEPTR(buf->prog);
		if (buf->pid != include_pid)
			FREEPTR(buf->pid);
		FREEPTR(buf->msgid);
		FREEPTR(buf->sd);
		FREEPTR(buf->msgorig);	/* instead of msg */
		FREEPTR(buf);
	}
}

size_t
buf_queue_obj_size(struct buf_queue *qentry)
{
	size_t sum = 0;

	if (!qentry)
		return 0;
	sum += sizeof(*qentry)
	    + sizeof(*qentry->msg)
	    + qentry->msg->msgsize
	    + SAFEstrlen(qentry->msg->timestamp)+1
	    + SAFEstrlen(qentry->msg->msgid)+1;
	if (qentry->msg->prog
	    && qentry->msg->prog != include_pid)
		sum += strlen(qentry->msg->prog)+1;
	if (qentry->msg->pid
	    && qentry->msg->pid != appname)
		sum += strlen(qentry->msg->pid)+1;
	if (qentry->msg->recvhost
	    && qentry->msg->recvhost != LocalHostName
	    && qentry->msg->recvhost != LocalFQDN
	    && qentry->msg->recvhost != oldLocalFQDN)
		sum += strlen(qentry->msg->recvhost)+1;
	if (qentry->msg->host
	    && qentry->msg->host != LocalHostName
	    && qentry->msg->host != LocalFQDN
	    && qentry->msg->host != oldLocalFQDN)
		sum += strlen(qentry->msg->host)+1;

	return sum;
}

bool
message_queue_remove(struct filed *f, struct buf_queue *qentry)
{
	if (!f || !qentry || !qentry->msg)
		return false;

	assert(!STAILQ_EMPTY(&f->f_qhead));
	STAILQ_REMOVE(&f->f_qhead, qentry, buf_queue, entries);
	f->f_qelements--;
	f->f_qsize -= buf_queue_obj_size(qentry);

	DPRINTF(D_BUFFER, "msg @%p removed from queue @%p, new qlen = %zu\n",
	    qentry->msg, f, f->f_qelements);
	DELREF(qentry->msg);
	FREEPTR(qentry);
	return true;
}

/*
 * returns *qentry on success and NULL on error
 */
struct buf_queue *
message_queue_add(struct filed *f, struct buf_msg *buffer)
{
	struct buf_queue *qentry;

	/* check on every call or only every n-th time? */
	message_queue_purge(f, 0, PURGE_BY_PRIORITY);

	while (!(qentry = malloc(sizeof(*qentry)))
	    && message_queue_purge(f, 1, PURGE_OLDEST))
		continue;
	if (!qentry) {
		logerror("Unable to allocate memory");
		DPRINTF(D_BUFFER, "queue empty, no memory, msg dropped\n");
		return NULL;
	} else {
		qentry->msg = buffer;
		f->f_qelements++;
		f->f_qsize += buf_queue_obj_size(qentry);
		STAILQ_INSERT_TAIL(&f->f_qhead, qentry, entries);

		DPRINTF(D_BUFFER, "msg @%p queued @%p, qlen = %zu\n",
		    buffer, f, f->f_qelements);
		return qentry;
	}
}

void
message_queue_freeall(struct filed *f)
{
	struct buf_queue *qentry;

	if (!f) return;
	DPRINTF(D_MEM, "message_queue_freeall(f@%p) with f_qhead@%p\n", f,
	    &f->f_qhead);

	while (!STAILQ_EMPTY(&f->f_qhead)) {
		qentry = STAILQ_FIRST(&f->f_qhead);
		STAILQ_REMOVE(&f->f_qhead, qentry, buf_queue, entries);
		DELREF(qentry->msg);
		FREEPTR(qentry);
	}

	f->f_qelements = 0;
	f->f_qsize = 0;
}

#ifndef DISABLE_TLS
/* utility function for tls_reconnect() */
struct filed *
get_f_by_conninfo(struct tls_conn_settings *conn_info)
{
	struct filed *f;

	for (f = Files; f; f = f->f_next) {
		if ((f->f_type == F_TLS) && f->f_un.f_tls.tls_conn == conn_info)
			return f;
	}
	DPRINTF(D_TLS, "get_f_by_conninfo() called on invalid conn_info\n");
	return NULL;
}

/*
 * Called on signal.
 * Lets the admin reconnect without waiting for the reconnect timer expires.
 */
/*ARGSUSED*/
void
dispatch_force_tls_reconnect(int fd, short event, void *ev)
{
	struct filed *f;
	DPRINTF((D_TLS|D_CALL|D_EVENT), "dispatch_force_tls_reconnect()\n");
	for (f = Files; f; f = f->f_next) {
		if (f->f_type == F_TLS &&
		    f->f_un.f_tls.tls_conn->state == ST_NONE)
			tls_reconnect(fd, event, f->f_un.f_tls.tls_conn);
	}
}
#endif /* !DISABLE_TLS */

/*
 * return a timestamp in a static buffer,
 * either format the timestamp given by parameter in_now
 * or use the current time if in_now is NULL.
 */
char *
make_timestamp(time_t *in_now, bool iso, size_t tlen)
{
	int frac_digits = 6;
	struct timeval tv;
	time_t mytime;
	struct tm ltime;
	int len = 0;
	int tzlen = 0;
	/* uses global var: time_t now; */

	if (in_now) {
		mytime = *in_now;
	} else {
		gettimeofday(&tv, NULL);
		mytime = now = tv.tv_sec;
	}

	if (!iso) {
		strlcpy(timestamp, ctime(&mytime) + 4, sizeof(timestamp));
		timestamp[BSD_TIMESTAMPLEN] = '\0';
	} else {
		localtime_r(&mytime, &ltime);
		len += strftime(timestamp, sizeof(timestamp), "%FT%T", &ltime);
		snprintf(&timestamp[len], frac_digits + 2, ".%.*jd",
		    frac_digits, (intmax_t)tv.tv_usec);
		len += frac_digits + 1;
		tzlen = strftime(&timestamp[len], sizeof(timestamp) - len, "%z",
		    &ltime);
		len += tzlen;

		if (tzlen == 5) {
			/* strftime gives "+0200", but we need "+02:00" */
			timestamp[len + 2] = '\0';
			timestamp[len + 1] = timestamp[len];
			timestamp[len] = timestamp[len - 1];
			timestamp[len - 1] = timestamp[len - 2];
			timestamp[len - 2] = ':';
		}
	}

	switch (tlen) {
	case (size_t)-1:
		return timestamp;
	case 0:
		return strdup(timestamp);
	default:
		return strndup(timestamp, tlen);
	}
}

/* auxillary code to allocate memory and copy a string */
bool
copy_string(char **mem, const char *p, const char *q)
{
	const size_t len = 1 + q - p;
	if (!(*mem = malloc(len))) {
		logerror("Unable to allocate memory for config");
		return false;
	}
	strlcpy(*mem, p, len);
	return true;
}

/* keyword has to end with ",  everything until next " is copied */
bool
copy_config_value_quoted(const char *keyword, char **mem, const char **p)
{
	const char *q;
	if (strncasecmp(*p, keyword, strlen(keyword)))
		return false;
	q = *p += strlen(keyword);
	if (!(q = strchr(*p, '"'))) {
		errno = 0;
		logerror("unterminated \"\n");
		return false;
	}
	if (!(copy_string(mem, *p, q)))
		return false;
	*p = ++q;
	return true;
}

/* for config file:
 * following = required but whitespace allowed, quotes optional
 * if numeric, then conversion to integer and no memory allocation
 */
bool
copy_config_value(const char *keyword, char **mem,
	const char **p, const char *file, int line)
{
	if (strncasecmp(*p, keyword, strlen(keyword)))
		return false;
	*p += strlen(keyword);

	while (isspace((unsigned char)**p))
		*p += 1;
	if (**p != '=') {
		errno = 0;
		logerror("expected \"=\" in file %s, line %d", file, line);
		return false;
	}
	*p += 1;

	return copy_config_value_word(mem, p);
}

/* copy next parameter from a config line */
bool
copy_config_value_word(char **mem, const char **p)
{
	const char *q;
	while (isspace((unsigned char)**p))
		*p += 1;
	if (**p == '"')
		return copy_config_value_quoted("\"", mem, p);

	/* without quotes: find next whitespace or end of line */
	(void)((q = strchr(*p, ' ')) || (q = strchr(*p, '\t'))
	     || (q = strchr(*p, '\n')) || (q = strchr(*p, '\0')));

	if (q-*p == 0 || !(copy_string(mem, *p, q)))
		return false;

	*p = ++q;
	return true;
}

static int
writev1(int fd, struct iovec *iov, size_t count)
{
	ssize_t nw = 0, tot = 0;
	size_t ntries = 5;

	if (count == 0)
		return 0;
	while (ntries--) {
		switch ((nw = writev(fd, iov, count))) {
		case -1:
			if (errno == EAGAIN || errno == EWOULDBLOCK) {
				struct pollfd pfd;
				pfd.fd = fd;
				pfd.events = POLLOUT;
				pfd.revents = 0;
				(void)poll(&pfd, 1, 500);
				continue;
			}
			return -1;
		case 0:
			return 0;
		default:
			tot += nw;
			while (nw > 0) {
				if (iov->iov_len > (size_t)nw) {
					iov->iov_len -= nw;
					iov->iov_base =
					    (char *)iov->iov_base + nw;
					break;
				} else {
					if (--count == 0)
						return tot;
					nw -= iov->iov_len;
					iov++;
				}
			}
		}
	}
	return tot == 0 ? nw : tot;
}

#ifndef NDEBUG
void
dbprintf(const char *fname, const char *funname,
    size_t lnum, const char *fmt, ...)
{
	va_list ap;
	char *ts;

	ts = make_timestamp(NULL, true, (size_t)-1);
	printf("%s:%s:%s:%.4zu\t", ts, fname, funname, lnum);

	va_start(ap, fmt);
	vprintf(fmt, ap);
	va_end(ap);
}
#endif
