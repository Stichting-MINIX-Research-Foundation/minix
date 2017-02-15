/*	$NetBSD: arp.c,v 1.53 2015/07/31 04:02:40 ozaki-r Exp $ */

/*
 * Copyright (c) 1984, 1993
 *	The Regents of the University of California.  All rights reserved.
 *
 * This code is derived from software contributed to Berkeley by
 * Sun Microsystems, Inc.
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
__COPYRIGHT("@(#) Copyright (c) 1984, 1993\
 The Regents of the University of California.  All rights reserved.");
#endif /* not lint */

#ifndef lint
#if 0
static char sccsid[] = "@(#)arp.c	8.3 (Berkeley) 4/28/95";
#else
__RCSID("$NetBSD: arp.c,v 1.53 2015/07/31 04:02:40 ozaki-r Exp $");
#endif
#endif /* not lint */

/*
 * arp - display, set, and delete arp table entries
 */

#include <sys/param.h>
#include <sys/file.h>
#include <sys/socket.h>
#include <sys/sysctl.h>
#include <sys/ioctl.h>

#include <net/if.h>
#include <net/if_dl.h>
#include <net/if_ether.h>
#include <net/if_types.h>
#include <net/route.h>
#include <netinet/in.h>
#include <netinet/if_inarp.h>
#include <arpa/inet.h>

#include <err.h>
#include <errno.h>
#include <netdb.h>
#include <nlist.h>
#include <paths.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <ifaddrs.h>

#include "prog_ops.h"

static int is_llinfo(const struct sockaddr_dl *, int);
static int delete(const char *, const char *);
static void dump(uint32_t);
static void delete_all(void);
static void sdl_print(const struct sockaddr_dl *);
static int getifname(u_int16_t, char *, size_t);
static int atosdl(const char *s, struct sockaddr_dl *sdl);
static int file(const char *);
static void get(const char *);
static int getinetaddr(const char *, struct in_addr *);
static int getsocket(void);
static struct rt_msghdr *
	rtmsg(const int, const int, const struct sockaddr_inarp *,
	    const struct sockaddr_dl *);
static int set(int, char **);
static void usage(void) __dead;

static int aflag, nflag, vflag;
static struct sockaddr_in so_mask = { 
	.sin_len = 8,
	.sin_addr = {
		.s_addr = 0xffffffff
	}
};
static struct sockaddr_inarp blank_sin = {
	.sin_len = sizeof(blank_sin),
	.sin_family = AF_INET
};
static struct sockaddr_dl blank_sdl = {
	.sdl_len = sizeof(blank_sdl),
	.sdl_family = AF_LINK
};

static int expire_time, flags, export_only, doing_proxy, found_entry;

int
main(int argc, char **argv)
{
	int ch;
	int op = 0;

	setprogname(argv[0]);

	while ((ch = getopt(argc, argv, "andsfv")) != -1)
		switch((char)ch) {
		case 'a':
			aflag = 1;
			break;
		case 'd':
		case 's':
		case 'f':
			if (op)
				usage();
			op = ch;
			break;
		case 'n':
			nflag = 1;
			break;
		case 'v':
			vflag = 1;
			break;
		default:
			usage();
		}
	argc -= optind;
	argv += optind;

	if (!op && aflag)
		op = 'a';

	if (prog_init && prog_init() == -1)
		err(1, "init failed");

	switch((char)op) {
	case 'a':
		dump(0);
		break;
	case 'd':
		if (aflag && argc == 0)
			delete_all();
		else {
			if (aflag || argc < 1 || argc > 2)
				usage();
			(void)delete(argv[0], argv[1]);
		}
		break;
	case 's':
		if (argc < 2 || argc > 7)
			usage();
		return (set(argc, argv) ? 1 : 0);
	case 'f':
		if (argc != 1)
			usage();
		return (file(argv[0]));
	default:
		if (argc != 1)
			usage();
		get(argv[0]);
		break;
	}
	return (0);
}

/*
 * Process a file to set standard arp entries
 */
static int
file(const char *name)
{
	char *line, *argv[5];
	int i, retval;
	FILE *fp;

	if (!strcmp(name, "-")) {
		fp = stdin;
	} else {
		fp = fopen(name, "r");
		if (fp == NULL) {
			err(1, "Cannot open %s", name);
		}
	}
	retval = 0;
	for (; (line = fparseln(fp, NULL, NULL, NULL, 0)) != NULL; free(line)) {
		char **ap, *inputstring;

		inputstring = line;
		for (ap = argv; ap < &argv[sizeof(argv) / sizeof(argv[0])] &&
		    (*ap = stresep(&inputstring, " \t", '\\')) != NULL;) {
		       if (**ap != '\0')
				ap++;
		}
		i = ap - argv;
		if (i < 2) {
			warnx("bad line: %s", line);
			retval = 1;
			continue;
		}
		if (set(i, argv))
			retval = 1;
	}
	if (fp != stdin)
		(void)fclose(fp);
	return retval;
}

static int
getsocket(void)
{
	int s;
	s = prog_socket(PF_ROUTE, SOCK_RAW, 0);
	if (s < 0)
		err(1, "socket");
	return s;
}

static int
getlink(const char *name, struct sockaddr_dl *sdl)
{
	struct ifaddrs *ifap, *ifa;

	if (getifaddrs(&ifap) != 0) {
		warn("getifaddrs");
		return 0;
	}

	for (ifa = ifap; ifa; ifa = ifa->ifa_next) {
		if (ifa->ifa_addr->sa_family != AF_LINK)
			continue;
		if (strcmp(ifa->ifa_name, name) != 0)
			continue;
		memcpy(sdl, ifa->ifa_addr, sizeof(*sdl));
		freeifaddrs(ifap);
		return 1;
	}
	freeifaddrs(ifap);
	return 0;
}

/*
 * Set an individual arp entry 
 */
static int
set(int argc, char **argv)
{
	struct sockaddr_inarp *sina;
	struct sockaddr_dl *sdl;
	struct rt_msghdr *rtm;
	char *host = argv[0], *eaddr;
	struct sockaddr_inarp sin_m = blank_sin; /* struct copy */
	struct sockaddr_dl sdl_m = blank_sdl; /* struct copy */
	int s;

	eaddr = argv[1];

	s = getsocket();
	argc -= 2;
	argv += 2;

	if (getinetaddr(host, &sin_m.sin_addr) == -1)
		return (1);
	if (atosdl(eaddr, &sdl_m))
		warnx("invalid link-level address '%s'", eaddr);
	doing_proxy = flags = export_only = expire_time = 0;
	for (; argc-- > 0; argv++) {
		if (strncmp(argv[0], "temp", 4) == 0) {
			struct timeval timev;
			(void)gettimeofday(&timev, 0);
			expire_time = timev.tv_sec + 20 * 60;
		}
		else if (strncmp(argv[0], "pub", 3) == 0) {
			flags |= RTF_ANNOUNCE;
			doing_proxy = SIN_PROXY;
			if (argc && strncmp(argv[1], "pro", 3) == 0) {
			        export_only = 1;
			        argc--; argv++;
			}
		} else if (strncmp(argv[0], "trail", 5) == 0) {
			warnx("%s: Sending trailers is no longer supported",
			    host);
		} else if (strcmp(argv[0], "ifscope") == 0) {
			if (argc == 0) {
				warnx("missing interface for ifscope");
				continue;
			}
			argc--;
			argv++;
			if (!getlink(argv[0], &sdl_m))
				warnx("cannot get link address for %s", argv[0]);
		}

	}
	if (memcmp(&sdl_m, &blank_sdl, sizeof(blank_sdl)))
		goto out;
tryagain:
	rtm = rtmsg(s, RTM_GET, &sin_m, &sdl_m);
	if (rtm == NULL) {
		warn("%s", host);
		return (1);
	}
	sina = (struct sockaddr_inarp *)(void *)(rtm + 1);
	sdl = (struct sockaddr_dl *)(void *)(RT_ROUNDUP(sina->sin_len) +
	    (char *)(void *)sina);
	if (sina->sin_addr.s_addr == sin_m.sin_addr.s_addr) {
		if (is_llinfo(sdl, rtm->rtm_flags))
			goto overwrite;
		if (doing_proxy == 0) {
			warnx("set: can only proxy for %s", host);
			return (1);
		}
		if (sin_m.sin_other & SIN_PROXY) {
			warnx("set: proxy entry exists for non 802 device");
			return (1);
		}
		sin_m.sin_other = SIN_PROXY;
		export_only = 1;
		goto tryagain;
	}
overwrite:
	if (sdl->sdl_family != AF_LINK) {
		warnx("cannot intuit interface index and type for %s",
		    host);
		return (1);
	}
	sdl_m.sdl_type = sdl->sdl_type;
	sdl_m.sdl_index = sdl->sdl_index;
out:
	sin_m.sin_other = 0;
	if (doing_proxy && export_only)
		sin_m.sin_other = SIN_PROXY;
	rtm = rtmsg(s, RTM_ADD, &sin_m, &sdl_m);
	if (vflag)
		(void)printf("%s (%s) added\n", host, eaddr);
	return (rtm == NULL) ? 1 : 0;
}

/*
 * Display an individual arp entry
 */
static void
get(const char *host)
{
	struct sockaddr_inarp sin = blank_sin; /* struct copy */

	if (getinetaddr(host, &sin.sin_addr) == -1)
		exit(1);
	dump(sin.sin_addr.s_addr);
	if (found_entry == 0)
		errx(1, "%s (%s) -- no entry", host, inet_ntoa(sin.sin_addr));
}


static int
is_llinfo(const struct sockaddr_dl *sdl, int rtflags)
{
	if (sdl->sdl_family != AF_LINK ||
	    (rtflags & (RTF_LLINFO|RTF_GATEWAY)) != RTF_LLINFO)
		return 0;

	switch (sdl->sdl_type) {
	case IFT_ETHER:
	case IFT_FDDI:
	case IFT_ISO88023:
	case IFT_ISO88024:
	case IFT_ISO88025:
	case IFT_ARCNET:
		return 1;
	default:
		return 0;
	}
}

/*
 * Delete an arp entry 
 */
int
delete(const char *host, const char *info)
{
	struct sockaddr_inarp *sina;
	struct sockaddr_dl *sdl;
	struct rt_msghdr *rtm;
	struct sockaddr_inarp sin_m = blank_sin; /* struct copy */
	struct sockaddr_dl sdl_m = blank_sdl; /* struct copy */
	int s;

	s = getsocket();
	if (info && strncmp(info, "pro", 3) == 0)
		 sin_m.sin_other = SIN_PROXY;
	if (getinetaddr(host, &sin_m.sin_addr) == -1)
		return (1);
tryagain:
	rtm = rtmsg(s, RTM_GET, &sin_m, &sdl_m);
	if (rtm == NULL) {
		warn("%s", host);
		return (1);
	}
	sina = (struct sockaddr_inarp *)(void *)(rtm + 1);
	sdl = (struct sockaddr_dl *)(void *)(RT_ROUNDUP(sina->sin_len) +
	    (char *)(void *)sina);
	if (sina->sin_addr.s_addr == sin_m.sin_addr.s_addr &&
	    is_llinfo(sdl, rtm->rtm_flags))
		goto delete;
	if (sin_m.sin_other & SIN_PROXY) {
		warnx("delete: can't locate %s", host);
		return (1);
	} else {
		sin_m.sin_other = SIN_PROXY;
		goto tryagain;
	}
delete:
	if (sdl->sdl_family != AF_LINK) {
		(void)warnx("cannot locate %s", host);
		return (1);
	}
	rtm = rtmsg(s, RTM_DELETE, &sin_m, sdl);
	if (rtm == NULL)
		return (1);
	if (vflag)
		(void)printf("%s (%s) deleted\n", host,
		    inet_ntoa(sin_m.sin_addr));
	return (0);
}

/*
 * Dump the entire arp table
 */
void
dump(uint32_t addr)
{
	int mib[6];
	size_t needed;
	char ifname[IFNAMSIZ];
	char *lim, *buf, *next;
        const char *host;
	struct rt_msghdr *rtm;
	struct sockaddr_inarp *sina;
	struct sockaddr_dl *sdl;
	struct hostent *hp;

	mib[0] = CTL_NET;
	mib[1] = PF_ROUTE;
	mib[2] = 0;
	mib[3] = AF_INET;
	mib[4] = NET_RT_FLAGS;
	mib[5] = RTF_LLINFO;
	if (prog_sysctl(mib, 6, NULL, &needed, NULL, 0) < 0)
		err(1, "route-sysctl-estimate");
	if (needed == 0)
		return;
	if ((buf = malloc(needed)) == NULL)
		err(1, "malloc");
	if (prog_sysctl(mib, 6, buf, &needed, NULL, 0) < 0)
		err(1, "actual retrieval of routing table");
	lim = buf + needed;
	for (next = buf; next < lim; next += rtm->rtm_msglen) {
		rtm = (struct rt_msghdr *)(void *)next;
		sina = (struct sockaddr_inarp *)(void *)(rtm + 1);
		sdl = (struct sockaddr_dl *)(void *)
		    (RT_ROUNDUP(sina->sin_len) + (char *)(void *)sina);
		if (addr) {
			if (addr != sina->sin_addr.s_addr)
				continue;
			found_entry = 1;
		}
		if (nflag == 0)
			hp = gethostbyaddr((const char *)(void *)
			    &(sina->sin_addr),
			    sizeof sina->sin_addr, AF_INET);
		else
			hp = NULL;

		host = hp ? hp->h_name : "?";

		(void)printf("%s (%s) at ", host, inet_ntoa(sina->sin_addr));
		if (sdl->sdl_alen)
			sdl_print(sdl);
		else
			(void)printf("(incomplete)");

		if (sdl->sdl_index) {
			if (getifname(sdl->sdl_index, ifname, sizeof(ifname)) == 0)
				(void)printf(" on %s", ifname);
		}

		if (rtm->rtm_rmx.rmx_expire == 0)
			(void)printf(" permanent");
		if (sina->sin_other & SIN_PROXY)
			(void)printf(" published (proxy only)");
		if (rtm->rtm_addrs & RTA_NETMASK) {
			sina = (struct sockaddr_inarp *)(void *)
			    (RT_ROUNDUP(sdl->sdl_len) + (char *)(void *)sdl);
			if (sina->sin_addr.s_addr == 0xffffffff)
				(void)printf(" published");
			if (sina->sin_len != 8)
				(void)printf("(weird)");
		}
		(void)printf("\n");
	}
	free(buf);
}

/*
 * Delete the entire arp table
 */
void
delete_all(void)
{
	int mib[6];
	size_t needed;
	char addr[sizeof("000.000.000.000\0")];
	char *lim, *buf, *next;
	struct rt_msghdr *rtm;
	struct sockaddr_inarp *sina;

	mib[0] = CTL_NET;
	mib[1] = PF_ROUTE;
	mib[2] = 0;
	mib[3] = AF_INET;
	mib[4] = NET_RT_FLAGS;
	mib[5] = RTF_LLINFO;
	if (prog_sysctl(mib, 6, NULL, &needed, NULL, 0) < 0)
		err(1, "route-sysctl-estimate");
	if (needed == 0)
		return;
	if ((buf = malloc(needed)) == NULL)
		err(1, "malloc");
	if (prog_sysctl(mib, 6, buf, &needed, NULL, 0) < 0)
		err(1, "actual retrieval of routing table");
	lim = buf + needed;
	for (next = buf; next < lim; next += rtm->rtm_msglen) {
		rtm = (struct rt_msghdr *)(void *)next;
		sina = (struct sockaddr_inarp *)(void *)(rtm + 1);
		(void)snprintf(addr, sizeof(addr), "%s",
		    inet_ntoa(sina->sin_addr));
		(void)delete(addr, NULL);
	}
	free(buf);
}

void
sdl_print(const struct sockaddr_dl *sdl)
{
	char hbuf[NI_MAXHOST];

	if (getnameinfo((const struct sockaddr *)(const void *)sdl,
	    (socklen_t)sdl->sdl_len,
	    hbuf, sizeof(hbuf), NULL, 0, NI_NUMERICHOST) != 0)
		(void)printf("<invalid>");
	else
		(void)printf("%s", hbuf);
}

static int
atosdl(const char *ss, struct sockaddr_dl *sdl)
{
	int i;
	unsigned long b;
	char *endp;
	char *p; 
	char *t, *r;

	p = LLADDR(sdl);
	endp = ((char *)(void *)sdl) + sdl->sdl_len;
	i = 0;
	
	b = strtoul(ss, &t, 16);
	if (b > 255 || t == ss)
		return 1;

	*p++ = (char)b;
	++i;
	while ((p < endp) && (*t++ == ':')) {
		b = strtoul(t, &r, 16);
		if (b > 255 || r == t)
			break;
		*p++ = (char)b;
		++i;
		t = r;
	}
	sdl->sdl_alen = i;

	return 0;
}

static void
usage(void)
{
	const char *progname;

	progname = getprogname();
	(void)fprintf(stderr, "Usage: %s [-n] hostname\n", progname);
	(void)fprintf(stderr, "	      %s [-nv] -a\n", progname);
	(void)fprintf(stderr, "	      %s [-v] -d [-a|hostname [pub [proxy]]]\n",
	    progname);
	(void)fprintf(stderr, "       %s -s hostname ether_addr [temp] [pub [proxy]]\n",
	    progname);
	(void)fprintf(stderr, "       %s -f filename\n", progname);
	exit(1);
}

static struct rt_msghdr *
rtmsg(const int s, const int cmd, const struct sockaddr_inarp *sin,
    const struct sockaddr_dl *sdl)
{
	static int seq;
	struct rt_msghdr *rtm;
	char *cp;
	int l;
	static struct {
		struct	rt_msghdr m_rtm;
		char	m_space[512];
	} m_rtmsg;
	pid_t pid;

	rtm = &m_rtmsg.m_rtm;
	cp = m_rtmsg.m_space;
	errno = 0;

	if (cmd == RTM_DELETE)
		goto doit;
	(void)memset(&m_rtmsg, 0, sizeof(m_rtmsg));
	rtm->rtm_flags = flags;
	rtm->rtm_version = RTM_VERSION;

	switch (cmd) {
	default:
		errx(1, "internal wrong cmd");
		/*NOTREACHED*/
	case RTM_ADD:
		rtm->rtm_addrs |= RTA_GATEWAY;
		rtm->rtm_rmx.rmx_expire = expire_time;
		rtm->rtm_inits = RTV_EXPIRE;
		rtm->rtm_flags |= (RTF_HOST | RTF_STATIC);
		if (doing_proxy) {
			if (!export_only) {
				rtm->rtm_addrs |= RTA_NETMASK;
				rtm->rtm_flags &= ~RTF_HOST;
			}
		}
		/* FALLTHROUGH */
	case RTM_GET:
		rtm->rtm_addrs |= RTA_DST;
	}

#define NEXTADDR(w, s) \
	if (rtm->rtm_addrs & (w)) { \
		(void)memcpy(cp, &s, \
		    (size_t)((const struct sockaddr *)&s)->sa_len); \
		RT_ADVANCE(cp, ((const struct sockaddr *)&s)); \
	}

	NEXTADDR(RTA_DST, *sin);
	NEXTADDR(RTA_GATEWAY, *sdl);
	NEXTADDR(RTA_NETMASK, so_mask);

	rtm->rtm_msglen = cp - (char *)(void *)&m_rtmsg;
doit:
	l = rtm->rtm_msglen;
	rtm->rtm_seq = ++seq;
	rtm->rtm_type = cmd;
#ifdef __minix
	/*
	 * Borrow from the future by setting the "this is a link-local request"
	 * flag on all routing socket requests.  IMPORTANT: this change may be
	 * dropped with the resync to NetBSD 8 as it will do the same thing,
	 * although slightly differently (and hence may not create a conflict).
	 */
	rtm->rtm_flags |= RTF_LLDATA;
#endif /* __minix */
	if (prog_write(s, &m_rtmsg, (size_t)l) < 0) {
		if (errno != ESRCH || cmd != RTM_DELETE) {
			warn("writing to routing socket");
			return NULL;
		}
	}

	pid = prog_getpid();
	do {
		l = prog_read(s, &m_rtmsg, sizeof(m_rtmsg));
	} while (l > 0 && (rtm->rtm_seq != seq || rtm->rtm_pid != pid));
	if (l < 0)
		warn("read from routing socket");
	return rtm;
}

static int
getinetaddr(const char *host, struct in_addr *inap)
{
	struct hostent *hp;

	if (inet_aton(host, inap) == 1)
		return (0);
	if ((hp = gethostbyname(host)) == NULL) {
		warnx("%s: %s", host, hstrerror(h_errno));
		return (-1);
	}
	(void)memcpy(inap, hp->h_addr, sizeof(*inap));
	return (0);
}

static int
getifname(u_int16_t ifindex, char *ifname, size_t l)
{
	int i;
	struct ifaddrs *addr;
	const struct sockaddr_dl *sdl = NULL;
	static struct ifaddrs* ifaddrs = NULL;

	if (ifaddrs == NULL) {
		i = getifaddrs(&ifaddrs);
		if (i != 0)
			err(1, "getifaddrs");
	}

	for (addr = ifaddrs; addr; addr = addr->ifa_next) {
		if (addr->ifa_addr == NULL || 
		    addr->ifa_addr->sa_family != AF_LINK)
			continue;

		sdl = (const struct sockaddr_dl *)(void *)addr->ifa_addr;
		if (sdl && sdl->sdl_index == ifindex) {
			(void) strlcpy(ifname, addr->ifa_name, l);
			return 0;
		}
	}

	return -1;
}
