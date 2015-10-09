/*	$NetBSD: w.c,v 1.82 2014/12/22 15:24:14 dennis Exp $	*/

/*-
 * Copyright (c) 1980, 1991, 1993, 1994
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
__COPYRIGHT("@(#) Copyright (c) 1980, 1991, 1993, 1994\
 The Regents of the University of California.  All rights reserved.");
#endif /* not lint */

#ifndef lint
#if 0
static char sccsid[] = "@(#)w.c	8.6 (Berkeley) 6/30/94";
#else
__RCSID("$NetBSD: w.c,v 1.82 2014/12/22 15:24:14 dennis Exp $");
#endif
#endif /* not lint */

/*
 * w - print system status (who and what)
 *
 * This program is similar to the systat command on Tenex/Tops 10/20
 *
 */
#include <sys/param.h>
#include <sys/types.h>
#include <sys/time.h>
#include <sys/stat.h>
#include <sys/sysctl.h>
#include <sys/proc.h>
#include <sys/ioctl.h>
#include <sys/socket.h>

#include <netinet/in.h>
#include <arpa/inet.h>

#include <ctype.h>
#include <err.h>
#include <errno.h>
#include <fcntl.h>
#include <kvm.h>
#include <limits.h>
#include <netdb.h>
#include <nlist.h>
#include <paths.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <tzfile.h>
#include <unistd.h>
#ifdef SUPPORT_UTMP
#include <utmp.h>
#endif
#ifdef SUPPORT_UTMPX
#include <utmpx.h>
#endif
#include <vis.h>

#include "extern.h"

struct timeval	boottime;
struct winsize	ws;
kvm_t	       *kd;
time_t		now;		/* the current time of day */
int		ttywidth;	/* width of tty */
int		argwidth;	/* width of tty left to print process args */
int		header = 1;	/* true if -h flag: don't print heading */
int		nflag;		/* true if -n flag: don't convert addrs */
int		wflag;		/* true if -w flag: wide printout */
int		sortidle;	/* sort bu idle time */
char	       *sel_user;	/* login of particular user selected */
char		domain[MAXHOSTNAMELEN + 1];
int maxname = 8, maxline = 3, maxhost = 16;

/*
 * One of these per active utmp entry.
 */
struct	entry {
	struct	entry *next;
	char name[UTX_USERSIZE + 1];
	char line[UTX_LINESIZE + 1];
	char host[UTX_HOSTSIZE + 1];
	char type[2];
	struct timeval tv;
	dev_t	tdev;			/* dev_t of terminal */
	time_t	idle;			/* idle time of terminal in seconds */
	struct	kinfo_proc2 *tp;	/* `most interesting' tty proc */
	struct	kinfo_proc2 *pp;	/* pid proc */
	pid_t	pid;			/* pid or ~0 if not known */
} *ehead = NULL, **nextp = &ehead;

static void	pr_args(struct kinfo_proc2 *);
static void	pr_header(time_t *, int);
static int	proc_compare_wrapper(const struct kinfo_proc2 *,
    const struct kinfo_proc2 *);
#if defined(SUPPORT_UTMP) || defined(SUPPORT_UTMPX)
static int	ttystat(const char *, struct stat *);
static void	process(struct entry *);
#endif
static void	fixhost(struct entry *ep);
__dead static void	usage(int);

int
main(int argc, char **argv)
{
	struct kinfo_proc2 *kp;
	struct entry *ep;
	int ch, i, nentries, nusers, wcmd, curtain, use_sysctl;
	char *memf, *nlistf, *usrnp;
	const char *options;
	time_t then;
	size_t len;
#ifdef SUPPORT_UTMP
	struct utmp *ut;
#endif
#ifdef SUPPORT_UTMPX
	struct utmpx *utx;
#endif
	const char *progname;
	char errbuf[_POSIX2_LINE_MAX];

	setprogname(argv[0]);

	/* Are we w(1) or uptime(1)? */
	progname = getprogname();
	if (*progname == '-')
		progname++;
	if (*progname == 'u') {
		wcmd = 0;
		options = "";
	} else {
		wcmd = 1;
		options = "hiM:N:nw";
	}

	memf = nlistf = NULL;
	while ((ch = getopt(argc, argv, options)) != -1)
		switch (ch) {
		case 'h':
			header = 0;
			break;
		case 'i':
			sortidle = 1;
			break;
		case 'M':
			header = 0;
			memf = optarg;
			break;
		case 'N':
			nlistf = optarg;
			break;
		case 'n':
			nflag = 1;
			break;
		case 'w':
			wflag = 1;
			break;
		case '?':
		default:
			usage(wcmd);
		}
	argc -= optind;
	argv += optind;

	use_sysctl = (memf == NULL && nlistf == NULL);

	if ((kd = kvm_openfiles(nlistf, memf, NULL,
	    memf == NULL ? KVM_NO_FILES : O_RDONLY, errbuf)) == NULL)
		errx(1, "%s", errbuf);

	(void)time(&now);

	if (use_sysctl) {
		len = sizeof(curtain);
		if (sysctlbyname("security.curtain", &curtain, &len, 
		    NULL, 0) == -1)
			curtain = 0;
	}

#ifdef SUPPORT_UTMPX
	setutxent();
#endif
#ifdef SUPPORT_UTMP
	setutent();
#endif

	if (*argv)
		sel_user = *argv;

	nusers = 0;
#ifdef SUPPORT_UTMPX
	while ((utx = getutxent()) != NULL) {
		if (utx->ut_type != USER_PROCESS)
			continue;
		++nusers;

#ifndef SUPPORT_UTMP
		if (wcmd == 0)
			continue;
#endif	/* !SUPPORT_UTMP */

		if (sel_user &&
		    strncmp(utx->ut_name, sel_user, sizeof(utx->ut_name)) != 0)
			continue;
		if ((ep = calloc(1, sizeof(struct entry))) == NULL)
			err(1, NULL);
		(void)memcpy(ep->line, utx->ut_line, sizeof(utx->ut_line));
		ep->line[sizeof(utx->ut_line)] = '\0';
		*nextp = ep;
		nextp = &(ep->next);

		if (wcmd == 0)
			continue;

		(void)memcpy(ep->name, utx->ut_name, sizeof(utx->ut_name));
		ep->name[sizeof(utx->ut_name)] = '\0';
		if (!nflag || getnameinfo((struct sockaddr *)&utx->ut_ss,
		    utx->ut_ss.ss_len, ep->host, sizeof(ep->host), NULL, 0,
		    NI_NUMERICHOST) != 0) {
			(void)memcpy(ep->host, utx->ut_host,
			    sizeof(utx->ut_host));
			ep->host[sizeof(utx->ut_host)] = '\0';
		}
		fixhost(ep);
		ep->type[0] = 'x';
		ep->tv = utx->ut_tv;
		ep->pid = utx->ut_pid;
		process(ep);
	}
#endif

#ifdef SUPPORT_UTMP
	while ((ut = getutent()) != NULL) {
		if (ut->ut_name[0] == '\0')
			continue;

		if (sel_user &&
		    strncmp(ut->ut_name, sel_user, sizeof(ut->ut_name)) != 0)
			continue;

		/* Don't process entries that we have utmpx for */
		for (ep = ehead; ep != NULL; ep = ep->next) {
			if (strncmp(ep->line, ut->ut_line,
			    sizeof(ut->ut_line)) == 0)
				break;
		}
		if (ep != NULL)
			continue;

		++nusers;

		if (wcmd == 0)
			continue;

		if ((ep = calloc(1, sizeof(struct entry))) == NULL)
			err(1, NULL);
		(void)memcpy(ep->name, ut->ut_name, sizeof(ut->ut_name));
		(void)memcpy(ep->line, ut->ut_line, sizeof(ut->ut_line));
		(void)memcpy(ep->host, ut->ut_host, sizeof(ut->ut_host));
		ep->name[sizeof(ut->ut_name)] = '\0';
		ep->line[sizeof(ut->ut_line)] = '\0';
		ep->host[sizeof(ut->ut_host)] = '\0';
		fixhost(ep);
		ep->tv.tv_sec = ut->ut_time;
		*nextp = ep;
		nextp = &(ep->next);
		process(ep);
	}
#endif

#ifdef SUPPORT_UTMPX
	endutxent();
#endif
#ifdef SUPPORT_UTMP
	endutent();
#endif

	if (header || wcmd == 0) {
		pr_header(&now, nusers);
		if (wcmd == 0)
			exit (0);
	}

	if ((kp = kvm_getproc2(kd, KERN_PROC_ALL, 0,
	    sizeof(struct kinfo_proc2), &nentries)) == NULL)
		errx(1, "%s", kvm_geterr(kd));

	/* Include trailing space because TTY header starts one column early. */
	for (i = 0; i < nentries; i++, kp++) {

		for (ep = ehead; ep != NULL; ep = ep->next) {
			if (ep->tdev != 0 && ep->tdev == kp->p_tdev &&
			    kp->p__pgid == kp->p_tpgid) {
				/*
				 * Proc is in foreground of this
				 * terminal
				 */
				if (proc_compare_wrapper(ep->tp, kp))
					ep->tp = kp;
				break;
			} 
			if (ep->pid != 0 && ep->pid == kp->p_pid) {
				ep->pp = kp;
				break;
			}
		}
	}

	if ((ioctl(STDOUT_FILENO, TIOCGWINSZ, &ws) == -1 &&
	    ioctl(STDERR_FILENO, TIOCGWINSZ, &ws) == -1 &&
	    ioctl(STDIN_FILENO, TIOCGWINSZ, &ws) == -1) || ws.ws_col == 0)
		ttywidth = 79;
	else
		ttywidth = ws.ws_col - 1;

	if (!wflag && maxhost > (ttywidth / 3))
		maxhost = ttywidth / 3;

	argwidth = printf("%-*s TTY     %-*s %*s  IDLE WHAT\n",
	    maxname, "USER", maxhost, "FROM",
	    7 /* "dddhhXm" */, "LOGIN@");
	argwidth -= sizeof("WHAT\n") - 1 /* NUL */;
	argwidth = ttywidth - argwidth;
	if (argwidth < 4)
		argwidth = 8;
	if (wflag)
		argwidth = -1;

	/* sort by idle time */
	if (sortidle && ehead != NULL) {
		struct entry *from = ehead, *save;

		ehead = NULL;
		while (from != NULL) {
			for (nextp = &ehead;
			    (*nextp) && from->idle >= (*nextp)->idle;
			    nextp = &(*nextp)->next)
				continue;
			save = from;
			from = from->next;
			save->next = *nextp;
			*nextp = save;
		}
	}
#if defined(SUPPORT_UTMP) && defined(SUPPORT_UTMPX)
	else if (ehead != NULL) {
		struct entry *from = ehead, *save;

		ehead = NULL;
		while (from != NULL) {
			for (nextp = &ehead;
			    (*nextp) && strcmp(from->line, (*nextp)->line) > 0;
			    nextp = &(*nextp)->next)
				continue;
			save = from;
			from = from->next;
			save->next = *nextp;
			*nextp = save;
		}
	}
#endif

	if (!nflag) {
		int	rv;
		char	*p;

		rv = gethostname(domain, sizeof(domain));
		domain[sizeof(domain) - 1] = '\0';
		if (rv < 0 || (p = strchr(domain, '.')) == 0)
			domain[0] = '\0';
		else
			memmove(domain, p, strlen(p) + 1);
	}

	for (ep = ehead; ep != NULL; ep = ep->next) {
		if (ep->tp != NULL)
			kp = ep->tp;
		else if (ep->pp != NULL)
			kp = ep->pp;
		else if (ep->pid != 0) {
			if (curtain)
				kp = NULL;
			else {
				warnx("Stale utmp%s entry: %s %s %s",
				    ep->type, ep->name, ep->line, ep->host);
				continue;
			}
		}
#if !defined(__minix)
		usrnp = (kp == NULL) ? ep->name : kp->p_login;
#else
		usrnp = ep->name; /* TODO: implement getlogin/setlogin */
#endif /* !defined(__minix) */
		(void)printf("%-*s %-7.7s %-*.*s ",
		    maxname, usrnp, ep->line,
		    maxhost, maxhost, ep->host);
		then = (time_t)ep->tv.tv_sec;
		pr_attime(&then, &now);
		pr_idle(ep->idle);
		pr_args(kp);
		(void)printf("\n");
	}
	exit(0);
}

static void
pr_args(struct kinfo_proc2 *kp)
{
	char **argv;
	int left;

	if (kp == 0)
		goto nothing;
	left = argwidth;
	argv = kvm_getargv2(kd, kp, (argwidth < 0) ? 0 : argwidth);
	if (argv == 0) {
		fmt_putc('(', &left);
		fmt_puts((char *)kp->p_comm, &left);
		fmt_putc(')', &left);
		return;
	}
	while (*argv) {
		fmt_puts(*argv, &left);
		argv++;
		fmt_putc(' ', &left);
	}
	return;
nothing:
	putchar('-');
}

static void
pr_header(time_t *nowp, int nusers)
{
	double avenrun[3];
	time_t uptime;
	int days, hrs, mins;
	int mib[2];
	size_t size, i;
	char buf[256];

	/*
	 * Print time of day.
	 *
	 * SCCS forces the string manipulation below, as it replaces
	 * %, M, and % in a character string with the file name.
	 */
	(void)strftime(buf, sizeof(buf), "%l:%" "M%p", localtime(nowp));
	buf[sizeof(buf) - 1] = '\0';
	(void)printf("%s ", buf);

	/*
	 * Print how long system has been up.
	 * (Found by looking getting "boottime" from the kernel)
	 */
	mib[0] = CTL_KERN;
	mib[1] = KERN_BOOTTIME;
	size = sizeof(boottime);
	if (sysctl(mib, 2, &boottime, &size, NULL, 0) != -1 &&
	    boottime.tv_sec != 0) {
		uptime = now - boottime.tv_sec;
		uptime += 30;
		if (uptime > SECSPERMIN) {
			days = uptime / SECSPERDAY;
			uptime %= SECSPERDAY;
			hrs = uptime / SECSPERHOUR;
			uptime %= SECSPERHOUR;
			mins = uptime / SECSPERMIN;
			(void)printf(" up");
			if (days > 0)
				(void)printf(" %d day%s,", days,
				    days > 1 ? "s" : "");
			if (hrs > 0 && mins > 0)
				(void)printf(" %2d:%02d,", hrs, mins);
			else {
				if (hrs > 0)
					(void)printf(" %d hr%s,",
					    hrs, hrs > 1 ? "s" : "");
				if (mins > 0)
					(void)printf(" %d min%s,",
					    mins, mins > 1 ? "s" : "");
			}
		}
	}

	/* Print number of users logged in to system */
	(void)printf(" %d user%s", nusers, nusers != 1 ? "s" : "");

	/*
	 * Print 1, 5, and 15 minute load averages.
	 */
	if (getloadavg(avenrun, sizeof(avenrun) / sizeof(avenrun[0])) == -1)
		(void)printf(", no load average information available\n");
	else {
		(void)printf(", load averages:");
		for (i = 0; i < (sizeof(avenrun) / sizeof(avenrun[0])); i++) {
			if (i > 0)
				(void)printf(",");
			(void)printf(" %.2f", avenrun[i]);
		}
		(void)printf("\n");
	}
}

#if defined(SUPPORT_UTMP) || defined(SUPPORT_UTMPX)
static int
ttystat(const char *line, struct stat *st)
{
	char ttybuf[MAXPATHLEN];

	(void)snprintf(ttybuf, sizeof(ttybuf), "%s%s", _PATH_DEV, line);
	return stat(ttybuf, st);
}

static void
process(struct entry *ep)
{
	struct stat st;
	time_t touched;
	int max;

	if ((max = strlen(ep->name)) > maxname)
		maxname = max;
	if ((max = strlen(ep->line)) > maxline)
		maxline = max;
	if ((max = strlen(ep->host)) > maxhost)
		maxhost = max;

	ep->tdev = 0;
	ep->idle = (time_t)-1;

#ifdef SUPPORT_UTMP
	/*
	 * Hack to recognize and correctly parse
	 * ut entry made by ftpd. The "tty" used
	 * by ftpd is not a real tty, just identifier in
	 * form ftpPID. Pid parsed from the "tty name"
	 * is used later to match corresponding process.
	 * NB: This is only used for utmp entries. For utmpx,
	 * we already have the pid.
	 */
	if (ep->pid == 0 && strncmp(ep->line, "ftp", 3) == 0) {
		ep->pid = strtol(ep->line + 3, NULL, 10);
		return;
	}
#endif
	if (ttystat(ep->line, &st) == -1)
		return;

	ep->tdev = st.st_rdev;
	/*
	 * If this is the console device, attempt to ascertain
	 * the true console device dev_t.
	 */
	if (ep->tdev == 0) {
		int mib[2];
		size_t size;

		mib[0] = CTL_KERN;
		mib[1] = KERN_CONSDEV;
		size = sizeof(dev_t);
		(void) sysctl(mib, 2, &ep->tdev, &size, NULL, 0);
	}

	touched = st.st_atime;
	if (touched < ep->tv.tv_sec) {
		/* tty untouched since before login */
		touched = ep->tv.tv_sec;
	}
	if ((ep->idle = now - touched) < 0)
		ep->idle = 0;
}
#endif

static int
proc_compare_wrapper(const struct kinfo_proc2 *p1,
    const struct kinfo_proc2 *p2)
{
	struct kinfo_lwp *l1, *l2;
	int cnt;

	if (p1 == NULL)
		return 1;

	l1 = kvm_getlwps(kd, p1->p_pid, 0, sizeof(*l1), &cnt);
	if (l1 == NULL || cnt == 0)
		return 1;

	l2 = kvm_getlwps(kd, p2->p_pid, 0, sizeof(*l1), &cnt);
	if (l2 == NULL || cnt == 0)
		return 0;

	return proc_compare(p1, l1, p2, l2);
}

static void
fixhost(struct entry *ep)
{
	char host_buf[sizeof(ep->host)];
	char *p, *x;
	struct hostent *hp;
	struct in_addr l;

	strlcpy(host_buf, *ep->host ? ep->host : "-", sizeof(host_buf));
	p = host_buf;

	/*
	 * XXX: Historical behavior, ':' in hostname means X display number,
	 * IPv6 not handled.
	 */
	for (x = p; x < &host_buf[sizeof(host_buf)]; x++)
		if (*x == '\0' || *x == ':')
			break;
	if (x == p + sizeof(host_buf) || *x != ':')
		x = NULL;
	else
		*x++ = '\0';

	if (!nflag && inet_aton(p, &l) &&
	    (hp = gethostbyaddr((char *)&l, sizeof(l), AF_INET))) {
		if (domain[0] != '\0') {
			p = hp->h_name;
			p += strlen(hp->h_name);
			p -= strlen(domain);
			if (p > hp->h_name &&
			    strcasecmp(p, domain) == 0)
				*p = '\0';
		}
		p = hp->h_name;
	}

	if (x)
		(void)snprintf(ep->host, sizeof(ep->host), "%s:%s", p, x);
	else

		strlcpy(ep->host, p, sizeof(ep->host));
}

static void
usage(int wcmd)
{

	if (wcmd)
		(void)fprintf(stderr,
		    "Usage: %s [-hinw] [-M core] [-N system] [user]\n",
		    getprogname());
	else
		(void)fprintf(stderr, "Usage: %s\n", getprogname());
	exit(1);
}
