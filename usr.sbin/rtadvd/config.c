/*	$NetBSD: config.c,v 1.34 2015/06/05 14:09:20 roy Exp $	*/
/*	$KAME: config.c,v 1.93 2005/10/17 14:40:02 suz Exp $	*/

/*
 * Copyright (C) 1998 WIDE Project.
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
 * 3. Neither the name of the project nor the names of its contributors
 *    may be used to endorse or promote products derived from this software
 *    without specific prior written permission.
 * 
 * THIS SOFTWARE IS PROVIDED BY THE PROJECT AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE PROJECT OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 */

#include <sys/param.h>
#include <sys/ioctl.h>
#include <sys/socket.h>
#include <sys/time.h>
#include <sys/sysctl.h>

#include <net/if.h>
#include <net/route.h>
#include <net/if_dl.h>
#ifdef __FreeBSD__
#include <net/if_var.h>
#endif

#include <netinet/in.h>
#include <netinet/in_var.h>
#include <netinet/ip6.h>
#include <netinet6/ip6_var.h>
#include <netinet/icmp6.h>
#include <netinet6/nd6.h>

#include <arpa/inet.h>

#include <stdio.h>
#include <syslog.h>
#include <errno.h>
#include <string.h>
#include <stdlib.h>
#include <search.h>
#include <unistd.h>
#include <ifaddrs.h>
#include <inttypes.h>

#include "rtadvd.h"
#include "advcap.h"
#include "timer.h"
#include "if.h"
#include "config.h"

#ifndef __arraycount
#define __arraycount(__x)	(sizeof(__x) / sizeof(__x[0]))
#endif

static time_t prefix_timo = (60 * 120);	/* 2 hours.
					 * XXX: should be configurable. */
static struct rtadvd_timer *prefix_timeout(void *);
static void makeentry(char *, size_t, int, const char *);
static int getinet6sysctl(int);

static size_t
encode_domain(char *dst, const char *src)
{
	ssize_t len;
	char *odst, *p;

	odst = dst;
	while (src && (len = strlen(src)) != 0) {
		p = strchr(src, '.');
		*dst++ = len = MIN(63, p == NULL ? len : p - src);
		memcpy(dst, src, len);
		dst += len;
		if (p == NULL)
			break;
		src = p + 1;
	}
	*dst++ = '\0';
	
	return dst - odst;
}

void
free_rainfo(struct rainfo *rai)
{
	struct prefix *pfx;
	struct rtinfo *rti;
	struct rdnss *rdnss;
	struct rdnss_addr *rdnsa;
	struct dnssl *dnssl;
	struct dnssl_domain *dnsd;

	rtadvd_remove_timer(&rai->timer);

	while ((pfx = TAILQ_FIRST(&rai->prefix))) {
		TAILQ_REMOVE(&rai->prefix, pfx, next);
		free(pfx);
	}

	while ((rti = TAILQ_FIRST(&rai->route))) {
		TAILQ_REMOVE(&rai->route, rti, next);
		free(rti);
	}

	while ((rdnss = TAILQ_FIRST(&rai->rdnss))) {
		TAILQ_REMOVE(&rai->rdnss, rdnss, next);
		while ((rdnsa = TAILQ_FIRST(&rdnss->list))) {
			TAILQ_REMOVE(&rdnss->list, rdnsa, next);
			free(rdnsa);
		}
		free(rdnss);
	}

	while ((dnssl = TAILQ_FIRST(&rai->dnssl))) {
		TAILQ_REMOVE(&rai->dnssl, dnssl, next);
		while ((dnsd = TAILQ_FIRST(&dnssl->list))) {
			TAILQ_REMOVE(&dnssl->list, dnsd, next);
			free(dnsd);
		}
		free(dnssl);
	}

	free(rai->sdl);
	free(rai->ra_data);
	free(rai);
}

void
getconfig(const char *intface, int exithard)
{
	int stat, c, i;
	char tbuf[BUFSIZ];
	struct rainfo *tmp, *rai;
	int32_t val;
	int64_t val64;
	char buf[BUFSIZ];
	char *bp = buf;
	char *addr, *flagstr, *ap;
	static int forwarding = -1;
	char entbuf[256], abuf[256];
	struct rdnss *rdnss;
	struct dnssl *dnssl;

#define MUSTHAVE(var, cap)	\
    do {								\
	int64_t t;							\
	if ((t = agetnum(cap)) < 0) {					\
		fprintf(stderr, "rtadvd: need %s for interface %s\n",	\
			cap, intface);					\
		goto errexit;						\
	}								\
	var = t;							\
     } while (0)
#define MAYHAVE(var, cap, def)	\
     do {								\
	if ((var = agetnum(cap)) < 0)					\
		var = def;						\
     } while (0)
#define	ELM_MALLOC(p)					\
	do {								\
		p = calloc(1, sizeof(*p));				\
		if (p == NULL) {					\
			syslog(LOG_ERR, "<%s> calloc failed: %m",	\
			    __func__);					\
			goto errexit;					\
		}							\
	} while(/*CONSTCOND*/0)

	if (if_nametoindex(intface) == 0) {
		syslog(LOG_INFO, "<%s> interface %s not found, ignoring",
		       __func__, intface);
		return;
	}

	syslog(LOG_DEBUG, "<%s> loading configuration for interface %s",
	       __func__, intface);

	if ((stat = agetent(tbuf, intface)) <= 0) {
		memset(tbuf, 0, sizeof(tbuf));
		syslog(LOG_INFO,
		       "<%s> %s isn't defined in the configuration file"
		       " or the configuration file doesn't exist."
		       " Treat it as default",
		        __func__, intface);
	}

	ELM_MALLOC(tmp);
	TAILQ_INIT(&tmp->prefix);
	TAILQ_INIT(&tmp->route);
	TAILQ_INIT(&tmp->rdnss);
	TAILQ_INIT(&tmp->dnssl);

	/* check if we are allowed to forward packets (if not determined) */
	if (forwarding < 0) {
		if ((forwarding = getinet6sysctl(IPV6CTL_FORWARDING)) < 0)
			exit(1);
	}

	/* get interface information */
	if (agetflag("nolladdr"))
		tmp->advlinkopt = 0;
	else
		tmp->advlinkopt = 1;
	if (tmp->advlinkopt) {
		if ((tmp->sdl = if_nametosdl(intface)) == NULL) {
			syslog(LOG_ERR,
			       "<%s> can't get information of %s",
			       __func__, intface);
			goto errexit;
		}
		tmp->ifindex = tmp->sdl->sdl_index;
	} else {
		tmp->ifindex = if_nametoindex(intface);
		if (tmp->ifindex == 0) {
			syslog(LOG_ERR,
			       "<%s> can't get information of %s",
			       __func__, intface);
			goto errexit;
		}
	}
	tmp->ifflags = if_getflags(tmp->ifindex, 0);
	strlcpy(tmp->ifname, intface, sizeof(tmp->ifname));
	if ((tmp->phymtu = if_getmtu(intface)) == 0) {
		tmp->phymtu = IPV6_MMTU;
		syslog(LOG_WARNING,
		       "<%s> can't get interface mtu of %s. Treat as %d",
		       __func__, intface, IPV6_MMTU);
	}

	/*
	 * set router configuration variables.
	 */
	MAYHAVE(val, "maxinterval", DEF_MAXRTRADVINTERVAL);
	if (val < MIN_MAXINTERVAL || val > MAX_MAXINTERVAL) {
		syslog(LOG_ERR,
		       "<%s> maxinterval (%d) on %s is invalid "
		       "(must be between %u and %u)", __func__, val,
		       intface, MIN_MAXINTERVAL, MAX_MAXINTERVAL);
		goto errexit;
	}
	tmp->maxinterval = val;
	MAYHAVE(val, "mininterval", tmp->maxinterval/3);
	if (val < MIN_MININTERVAL || val > (tmp->maxinterval * 3) / 4) {
		syslog(LOG_ERR,
		       "<%s> mininterval (%d) on %s is invalid "
		       "(must be between %u and %d)",
		       __func__, val, intface, MIN_MININTERVAL,
		       (tmp->maxinterval * 3) / 4);
		goto errexit;
	}
	tmp->mininterval = val;

	MAYHAVE(val, "chlim", DEF_ADVCURHOPLIMIT);
	tmp->hoplimit = val & 0xff;

	if ((flagstr = (char *)agetstr("raflags", &bp))) {
		val = 0;
		if (strchr(flagstr, 'm'))
			val |= ND_RA_FLAG_MANAGED;
		if (strchr(flagstr, 'o'))
			val |= ND_RA_FLAG_OTHER;
		if (strchr(flagstr, 'h'))
			val |= ND_RA_FLAG_RTPREF_HIGH;
		if (strchr(flagstr, 'l')) {
			if ((val & ND_RA_FLAG_RTPREF_HIGH)) {
				syslog(LOG_ERR, "<%s> the \'h\' and \'l\'"
				    " router flags are exclusive", __func__);
				goto errexit;
			}
			val |= ND_RA_FLAG_RTPREF_LOW;
		}
	} else {
		MAYHAVE(val, "raflags", 0);
	}
	tmp->managedflg = val & ND_RA_FLAG_MANAGED;
	tmp->otherflg = val & ND_RA_FLAG_OTHER;
#ifndef ND_RA_FLAG_RTPREF_MASK
#define ND_RA_FLAG_RTPREF_MASK	0x18 /* 00011000 */
#define ND_RA_FLAG_RTPREF_RSV	0x10 /* 00010000 */
#endif
	tmp->rtpref = val & ND_RA_FLAG_RTPREF_MASK;
	if (tmp->rtpref == ND_RA_FLAG_RTPREF_RSV) {
		syslog(LOG_ERR, "<%s> invalid router preference (%02x) on %s",
		       __func__, tmp->rtpref, intface);
		goto errexit;
	}

	MAYHAVE(val, "rltime", tmp->maxinterval * 3);
	if (val && (val < tmp->maxinterval || val > MAXROUTERLIFETIME)) {
		syslog(LOG_ERR,
		       "<%s> router lifetime (%d) on %s is invalid "
		       "(must be 0 or between %d and %d)",
		       __func__, val, intface,
		       tmp->maxinterval, MAXROUTERLIFETIME);
		goto errexit;
	}
	/*
	 * Basically, hosts MUST NOT send Router Advertisement messages at any
	 * time (RFC 2461, Section 6.2.3). However, it would sometimes be
	 * useful to allow hosts to advertise some parameters such as prefix
	 * information and link MTU. Thus, we allow hosts to invoke rtadvd
	 * only when router lifetime (on every advertising interface) is
	 * explicitly set zero. (see also the above section)
	 */
	if (val && forwarding == 0) {
		syslog(LOG_ERR,
		       "<%s> non zero router lifetime is specified for %s, "
		       "which must not be allowed for hosts.  you must "
		       "change router lifetime or enable IPv6 forwarding.",
		       __func__, intface);
		goto errexit;
	}
	tmp->lifetime = val & 0xffff;

	MAYHAVE(val, "rtime", DEF_ADVREACHABLETIME);
	if (val < 0 || val > MAXREACHABLETIME) {
		syslog(LOG_ERR,
		       "<%s> reachable time (%d) on %s is invalid "
		       "(must be no greater than %d)",
		       __func__, val, intface, MAXREACHABLETIME);
		goto errexit;
	}
	tmp->reachabletime = (uint32_t)val;

	MAYHAVE(val64, "retrans", DEF_ADVRETRANSTIMER);
	if (val64 < 0 || val64 > 0xffffffff) {
		syslog(LOG_ERR, "<%s> retrans time (%lld) on %s out of range",
		       __func__, (long long)val64, intface);
		goto errexit;
	}
	tmp->retranstimer = (uint32_t)val64;

	if (agetnum("hapref") != -1 || agetnum("hatime") != -1) {
		syslog(LOG_ERR,
		       "<%s> mobile-ip6 configuration not supported",
		       __func__);
		goto errexit;
	}
	/* prefix information */

	/*
	 * This is an implementation specific parameter to consider
	 * link propagation delays and poorly synchronized clocks when
	 * checking consistency of advertised lifetimes.
	 */
	MAYHAVE(val, "clockskew", 0);
	tmp->clockskew = val;

	tmp->pfxs = 0;
	for (i = -1; i < MAXPREFIX; i++) {
		struct prefix *pfx;

		makeentry(entbuf, sizeof(entbuf), i, "addr");
		addr = (char *)agetstr(entbuf, &bp);
		if (addr == NULL)
			continue;

		/* allocate memory to store prefix information */
		if ((pfx = calloc(1, sizeof(*pfx))) == NULL) {
			syslog(LOG_ERR,
			       "<%s> can't allocate memory: %m",
			       __func__);
			goto errexit;
		}

		TAILQ_INSERT_TAIL(&tmp->prefix, pfx, next);
		tmp->pfxs++;
		pfx->rainfo = tmp;

		pfx->origin = PREFIX_FROM_CONFIG;

		if (inet_pton(AF_INET6, addr, &pfx->prefix) != 1) {
			syslog(LOG_ERR,
			       "<%s> inet_pton failed for %s",
			       __func__, addr);
			goto errexit;
		}
		if (IN6_IS_ADDR_MULTICAST(&pfx->prefix)) {
			syslog(LOG_ERR,
			       "<%s> multicast prefix (%s) must "
			       "not be advertised on %s",
			       __func__, addr, intface);
			goto errexit;
		}
		if (IN6_IS_ADDR_LINKLOCAL(&pfx->prefix))
			syslog(LOG_NOTICE,
			       "<%s> link-local prefix (%s) will be"
			       " advertised on %s",
			       __func__, addr, intface);

		makeentry(entbuf, sizeof(entbuf), i, "prefixlen");
		MAYHAVE(val, entbuf, 64);
		if (val < 0 || val > 128) {
			syslog(LOG_ERR, "<%s> prefixlen (%d) for %s "
			       "on %s out of range",
			       __func__, val, addr, intface);
			goto errexit;
		}
		pfx->prefixlen = (int)val;

		makeentry(entbuf, sizeof(entbuf), i, "pinfoflags");
		if ((flagstr = (char *)agetstr(entbuf, &bp))) {
			val = 0;
			if (strchr(flagstr, 'l'))
				val |= ND_OPT_PI_FLAG_ONLINK;
			if (strchr(flagstr, 'a'))
				val |= ND_OPT_PI_FLAG_AUTO;
		} else {
			MAYHAVE(val, entbuf,
			    (ND_OPT_PI_FLAG_ONLINK|ND_OPT_PI_FLAG_AUTO));
		}
		pfx->onlinkflg = val & ND_OPT_PI_FLAG_ONLINK;
		pfx->autoconfflg = val & ND_OPT_PI_FLAG_AUTO;

		makeentry(entbuf, sizeof(entbuf), i, "vltime");
		MAYHAVE(val64, entbuf, DEF_ADVVALIDLIFETIME);
		if (val64 < 0 || val64 > 0xffffffff) {
			syslog(LOG_ERR, "<%s> vltime (%lld) for "
			    "%s/%d on %s is out of range",
			    __func__, (long long)val64,
			    addr, pfx->prefixlen, intface);
			goto errexit;
		}
		pfx->validlifetime = (uint32_t)val64;

		makeentry(entbuf, sizeof(entbuf), i, "vltimedecr");
		if (agetflag(entbuf)) {
			struct timespec now;
			clock_gettime(CLOCK_MONOTONIC, &now);
			pfx->vltimeexpire =
				now.tv_sec + pfx->validlifetime;
		}

		makeentry(entbuf, sizeof(entbuf), i, "pltime");
		MAYHAVE(val64, entbuf, DEF_ADVPREFERREDLIFETIME);
		if (val64 < 0 || val64 > 0xffffffff) {
			syslog(LOG_ERR,
			    "<%s> pltime (%lld) for %s/%d on %s "
			    "is out of range",
			    __func__, (long long)val64,
			    addr, pfx->prefixlen, intface);
			goto errexit;
		}
		pfx->preflifetime = (uint32_t)val64;

		makeentry(entbuf, sizeof(entbuf), i, "pltimedecr");
		if (agetflag(entbuf)) {
			struct timespec now;
			clock_gettime(CLOCK_MONOTONIC, &now);
			pfx->pltimeexpire =
				now.tv_sec + pfx->preflifetime;
		}
	}
	if (TAILQ_FIRST(&tmp->prefix) == NULL && !agetflag("noifprefix"))
		get_prefix(tmp);

	MAYHAVE(val64, "mtu", 0);
	if (val64 < 0 || val64 > 0xffffffff) {
		syslog(LOG_ERR,
		       "<%s> mtu (%" PRIi64 ") on %s out of range",
		       __func__, val64, intface);
		goto errexit;
	}
	tmp->linkmtu = (uint32_t)val64;
	if (tmp->linkmtu == 0) {
		char *mtustr;

		if ((mtustr = (char *)agetstr("mtu", &bp)) &&
		    strcmp(mtustr, "auto") == 0)
			tmp->linkmtu = tmp->phymtu;
	}
	else if (tmp->linkmtu < IPV6_MMTU || tmp->linkmtu > tmp->phymtu) {
		syslog(LOG_ERR,
		       "<%s> advertised link mtu (%d) on %s is invalid (must "
		       "be between least MTU (%d) and physical link MTU (%d)",
		       __func__, tmp->linkmtu, intface,
		       IPV6_MMTU, tmp->phymtu);
		goto errexit;
	}

#ifdef SIOCSIFINFO_IN6
	{
		struct in6_ndireq ndi;
		int s;

		if ((s = socket(AF_INET6, SOCK_DGRAM, 0)) < 0) {
			syslog(LOG_ERR, "<%s> socket: %m", __func__);
			goto errexit;
		}
		memset(&ndi, 0, sizeof(ndi));
		strncpy(ndi.ifname, intface, IFNAMSIZ);
		if (ioctl(s, SIOCGIFINFO_IN6, &ndi) < 0) {
			syslog(LOG_INFO, "<%s> ioctl:SIOCGIFINFO_IN6 at %s: %m",
			     __func__, intface);
		}

		/* reflect the RA info to the host variables in kernel */
		ndi.ndi.chlim = tmp->hoplimit;
		ndi.ndi.retrans = tmp->retranstimer;
		ndi.ndi.basereachable = tmp->reachabletime;
		if (ioctl(s, SIOCSIFINFO_IN6, &ndi) < 0) {
			syslog(LOG_INFO, "<%s> ioctl:SIOCSIFINFO_IN6 at %s: %m",
			     __func__, intface);
		}
		close(s);
	}
#endif

	/* route information */
	for (i = -1; i < MAXROUTE; i++) {
		struct rtinfo *rti;
		char oentbuf[256];

		makeentry(entbuf, sizeof(entbuf), i, "rtprefix");
		addr = (char *)agetstr(entbuf, &bp);
		if (addr == NULL) {
			makeentry(oentbuf, sizeof(oentbuf), i, "rtrprefix");
			addr = (char *)agetstr(oentbuf, &bp);
			if (addr) {
				fprintf(stderr, "%s was obsoleted.  Use %s.\n",
					oentbuf, entbuf);
			}
		}
		if (addr == NULL)
			continue;

		ELM_MALLOC(rti);
		memset(rti, 0, sizeof(*rti));

		/* link into chain */
		TAILQ_INSERT_TAIL(&tmp->route, rti, next);

		if (inet_pton(AF_INET6, addr, &rti->prefix) != 1) {
			syslog(LOG_ERR, "<%s> inet_pton failed for %s",
			       __func__, addr);
			goto errexit;
		}
#if 0
		/*
		 * XXX: currently there's no restriction in route information
		 * prefix according to
		 * draft-ietf-ipngwg-router-selection-00.txt.
		 * However, I think the similar restriction be necessary.
		 */
		MAYHAVE(val64, entbuf, DEF_ADVVALIDLIFETIME);
		if (IN6_IS_ADDR_MULTICAST(&rti->prefix)) {
			syslog(LOG_ERR,
			       "<%s> multicast route (%s) must "
			       "not be advertised on %s",
			       __func__, addr, intface);
			goto errexit;
		}
		if (IN6_IS_ADDR_LINKLOCAL(&rti->prefix)) {
			syslog(LOG_NOTICE,
			       "<%s> link-local route (%s) will "
			       "be advertised on %s",
			       __func__, addr, intface);
			goto errexit;
		}
#endif

		makeentry(entbuf, sizeof(entbuf), i, "rtplen");
		/* XXX: 256 is a magic number for compatibility check. */
		MAYHAVE(val, entbuf, 256);
		if (val == 256) {
			makeentry(oentbuf, sizeof(oentbuf), i, "rtrplen");
			MAYHAVE(val, oentbuf, 256);
			if (val != 256) {
				fprintf(stderr, "%s was obsoleted.  Use %s.\n",
					oentbuf, entbuf);
			} else
				val = 64;
		}
		if (val < 0 || val > 128) {
			syslog(LOG_ERR, "<%s> prefixlen (%d) for %s on %s "
			       "out of range",
			       __func__, val, addr, intface);
			goto errexit;
		}
		rti->prefixlen = (int)val;

		makeentry(entbuf, sizeof(entbuf), i, "rtflags");
		if ((flagstr = (char *)agetstr(entbuf, &bp))) {
			val = 0;
			if (strchr(flagstr, 'h'))
				val |= ND_RA_FLAG_RTPREF_HIGH;
			if (strchr(flagstr, 'l')) {
				if ((val & ND_RA_FLAG_RTPREF_HIGH)) {
					syslog(LOG_ERR,
					    "<%s> the \'h\' and \'l\' route"
					    " preferences are exclusive",
					    __func__);
					goto errexit;
				}
				val |= ND_RA_FLAG_RTPREF_LOW;
			}
		} else
			MAYHAVE(val, entbuf, 256); /* XXX */
		if (val == 256) {
			makeentry(oentbuf, sizeof(oentbuf), i, "rtrflags");
			MAYHAVE(val, oentbuf, 256);
			if (val != 256) {
				fprintf(stderr, "%s was obsoleted.  Use %s.\n",
					oentbuf, entbuf);
			} else
				val = 0;
		}
		rti->rtpref = val & ND_RA_FLAG_RTPREF_MASK;
		if (rti->rtpref == ND_RA_FLAG_RTPREF_RSV) {
			syslog(LOG_ERR, "<%s> invalid route preference (%02x) "
			       "for %s/%d on %s",
			       __func__, rti->rtpref, addr,
			       rti->prefixlen, intface);
			goto errexit;
		}

		/*
		 * Since the spec does not a default value, we should make
		 * this entry mandatory.  However, FreeBSD 4.4 has shipped
		 * with this field being optional, we use the router lifetime
		 * as an ad-hoc default value with a warning message.
		 */
		makeentry(entbuf, sizeof(entbuf), i, "rtltime");
		MAYHAVE(val64, entbuf, -1);
		if (val64 == -1) {
			makeentry(oentbuf, sizeof(oentbuf), i, "rtrltime");
			MAYHAVE(val64, oentbuf, -1);
			if (val64 != -1) {
				fprintf(stderr, "%s was obsoleted.  Use %s.\n",
					oentbuf, entbuf);
			} else {
				fprintf(stderr, "%s should be specified "
					"for interface %s.\n",
					entbuf, intface);
				val64 = tmp->lifetime;
			}
		}
		if (val64 < 0 || val64 > 0xffffffff) {
			syslog(LOG_ERR, "<%s> route lifetime (%lld) for "
			    "%s/%d on %s out of range", __func__,
			    (long long)val64, addr, rti->prefixlen, intface);
			goto errexit;
		}
		rti->ltime = (uint32_t)val64;
	}

	/* RDNSS */
	for (i = -1; i < MAXRDNSS; i++) {
		struct rdnss_addr *rdnsa;

		makeentry(entbuf, sizeof(entbuf), i, "rdnss");
		addr = (char *)agetstr(entbuf, &bp);
		if (addr == NULL)
			continue;

		ELM_MALLOC(rdnss);
		TAILQ_INSERT_TAIL(&tmp->rdnss, rdnss, next);
		TAILQ_INIT(&rdnss->list);

		for (ap = addr; ap - addr < (ssize_t)strlen(addr); ap += c+1) {
			c = strcspn(ap, ",");
			strncpy(abuf, ap, c);
			abuf[c] = '\0';
			ELM_MALLOC(rdnsa);
			TAILQ_INSERT_TAIL(&rdnss->list, rdnsa, next);
			if (inet_pton(AF_INET6, abuf, &rdnsa->addr) != 1) {
				syslog(LOG_ERR, "<%s> inet_pton failed for %s",
			           __func__, addr);
				goto errexit;
			}
		}

		makeentry(entbuf, sizeof(entbuf), i, "rdnssltime");
		MAYHAVE(val64, entbuf, tmp->maxinterval * 3 / 2);
		if (val64 < tmp->maxinterval ||
		    val64 > tmp->maxinterval * 2)
		{
			syslog(LOG_ERR, "<%s> %s (%lld) on %s is invalid",
		    	     __func__, entbuf, (long long)val64, intface);
			goto errexit;
		}
		rdnss->lifetime = (uint32_t)val64;

	}

	/* DNSSL */
	TAILQ_INIT(&tmp->dnssl);
	for (i = -1; i < MAXDNSSL; i++) {
		struct dnssl_domain *dnsd;

		makeentry(entbuf, sizeof(entbuf), i, "dnssl");
		addr = (char *)agetstr(entbuf, &bp);
		if (addr == NULL)
			continue;

		ELM_MALLOC(dnssl);
		TAILQ_INSERT_TAIL(&tmp->dnssl, dnssl, next);
		TAILQ_INIT(&dnssl->list);

		for (ap = addr; ap - addr < (ssize_t)strlen(addr); ap += c+1) {
			c = strcspn(ap, ",");
			strncpy(abuf, ap, c);
			abuf[c] = '\0';
			ELM_MALLOC(dnsd);
			TAILQ_INSERT_TAIL(&dnssl->list, dnsd, next);
			dnsd->len = encode_domain(dnsd->domain, abuf);
		}

		makeentry(entbuf, sizeof(entbuf), i, "dnsslltime");
		MAYHAVE(val64, entbuf, tmp->maxinterval * 3 / 2);
		if (val64 < tmp->maxinterval ||
		    val64 > tmp->maxinterval * 2)
		{
			syslog(LOG_ERR, "<%s> %s (%lld) on %s is invalid",
		    	     __func__, entbuf, (long long)val64, intface);
			goto errexit;
		}
		dnssl->lifetime = (uint32_t)val64;

	}

	TAILQ_FOREACH(rai, &ralist, next) {
		if (rai->ifindex == tmp->ifindex) {
			TAILQ_REMOVE(&ralist, rai, next);
			/* If we already have a leaving RA use that
			 * as this config hasn't been advertised */
			if (rai->leaving) {
				tmp->leaving = rai->leaving;
				free_rainfo(rai);
				rai = tmp->leaving;
				rai->leaving_for = tmp;
				break;
			}
			rai->lifetime = 0;
			TAILQ_FOREACH(rdnss, &rai->rdnss, next)
				rdnss->lifetime = 0;
			TAILQ_FOREACH(dnssl, &rai->dnssl, next)
				dnssl->lifetime = 0;
			rai->leaving_for = tmp;
			tmp->leaving = rai;
			rai->initcounter = MAX_INITIAL_RTR_ADVERTISEMENTS;
			rai->mininterval = MIN_DELAY_BETWEEN_RAS;
			rai->maxinterval = MIN_DELAY_BETWEEN_RAS;
			rai->leaving_adv = MAX_FINAL_RTR_ADVERTISEMENTS;
			if (rai->timer == NULL)
				rai->timer = rtadvd_add_timer(ra_timeout,
							      ra_timer_update,
							      rai, rai);
			ra_timer_update((void *)rai, &rai->timer->tm);
			rtadvd_set_timer(&rai->timer->tm, rai->timer);
			break;
		}
	}

	/* okey */
	TAILQ_INSERT_TAIL(&ralist, tmp, next);

	/* construct the sending packet */
	make_packet(tmp);

	/* set timer */
	if (rai)
		return;
	tmp->timer = rtadvd_add_timer(ra_timeout, ra_timer_update,
				      tmp, tmp);
	ra_timer_set_short_delay(tmp);

	return;

errexit:
	if (exithard)
		exit(1);
	free_rainfo(tmp);
}

void
get_prefix(struct rainfo *rai)
{
	struct ifaddrs *ifap, *ifa;
	struct prefix *pp;
	struct in6_addr *a;
	unsigned char *p, *ep, *m, *lim;
	char ntopbuf[INET6_ADDRSTRLEN];

	if (getifaddrs(&ifap) < 0) {
		syslog(LOG_ERR,
		       "<%s> can't get interface addresses",
		       __func__);
		exit(1);
	}

	for (ifa = ifap; ifa; ifa = ifa->ifa_next) {
		int plen;

		if (strcmp(ifa->ifa_name, rai->ifname) != 0)
			continue;
		if (ifa->ifa_addr->sa_family != AF_INET6)
			continue;
		a = &((struct sockaddr_in6 *)ifa->ifa_addr)->sin6_addr;
		if (IN6_IS_ADDR_LINKLOCAL(a))
			continue;
		/* get prefix length */
		m = (unsigned char *)&((struct sockaddr_in6 *)ifa->ifa_netmask)->sin6_addr;
		lim = (unsigned char *)(ifa->ifa_netmask) + ifa->ifa_netmask->sa_len;
		plen = prefixlen(m, lim);
		if (plen <= 0 || plen > 128) {
			syslog(LOG_ERR, "<%s> failed to get prefixlen "
			       "or prefix is invalid",
			       __func__);
			exit(1);
		}
		if (plen == 128)	/* XXX */
			continue;
		if (find_prefix(rai, a, plen)) {
			/* ignore a duplicated prefix. */
			continue;
		}

		/* allocate memory to store prefix info. */
		if ((pp = calloc(1, sizeof(*pp))) == NULL) {
			syslog(LOG_ERR,
			       "<%s> can't get allocate buffer for prefix",
			       __func__);
			exit(1);
		}

		/* set prefix, sweep bits outside of prefixlen */
		pp->prefixlen = plen;
		memcpy(&pp->prefix, a, sizeof(*a));
		if (1)
		{
			p = (unsigned char *)&pp->prefix;
			ep = (unsigned char *)(&pp->prefix + 1);
			while (m < lim && p < ep)
				*p++ &= *m++;
			while (p < ep)
				*p++ = 0x00;
		}
	        if (!inet_ntop(AF_INET6, &pp->prefix, ntopbuf,
	            sizeof(ntopbuf))) {
			syslog(LOG_ERR, "<%s> inet_ntop failed", __func__);
			exit(1);
		}
		syslog(LOG_DEBUG,
		       "<%s> add %s/%d to prefix list on %s",
		       __func__, ntopbuf, pp->prefixlen, rai->ifname);

		/* set other fields with protocol defaults */
		pp->validlifetime = DEF_ADVVALIDLIFETIME;
		pp->preflifetime = DEF_ADVPREFERREDLIFETIME;
		pp->onlinkflg = 1;
		pp->autoconfflg = 1;
		pp->origin = PREFIX_FROM_KERNEL;
		pp->rainfo = rai;

		/* link into chain */
		TAILQ_INSERT_TAIL(&rai->prefix, pp, next);
		rai->pfxs++;
	}

	freeifaddrs(ifap);
}

static void
makeentry(char *buf, size_t len, int id, const char *string)
{

	if (id < 0)
		strlcpy(buf, string, len);
	else
		snprintf(buf, len, "%s%d", string, id);
}

/*
 * Add a prefix to the list of specified interface and reconstruct
 * the outgoing packet.
 * The prefix must not be in the list.
 * XXX: other parameters of the prefix(e.g. lifetime) should be
 * able to be specified.
 */
static void
add_prefix(struct rainfo *rai, struct in6_prefixreq *ipr)
{
	struct prefix *prefix;
	char ntopbuf[INET6_ADDRSTRLEN];

	if ((prefix = calloc(1, sizeof(*prefix))) == NULL) {
		syslog(LOG_ERR, "<%s> memory allocation failed",
		       __func__);
		return;		/* XXX: error or exit? */
	}
	prefix->prefix = ipr->ipr_prefix.sin6_addr;
	prefix->prefixlen = ipr->ipr_plen;
	prefix->validlifetime = ipr->ipr_vltime;
	prefix->preflifetime = ipr->ipr_pltime;
	prefix->onlinkflg = ipr->ipr_raf_onlink;
	prefix->autoconfflg = ipr->ipr_raf_auto;
	prefix->origin = PREFIX_FROM_DYNAMIC;

	prefix->rainfo = rai;
	TAILQ_INSERT_TAIL(&rai->prefix, prefix, next);
	rai->pfxs++;

	syslog(LOG_DEBUG, "<%s> new prefix %s/%d was added on %s",
	       __func__, inet_ntop(AF_INET6, &ipr->ipr_prefix.sin6_addr,
				       ntopbuf, INET6_ADDRSTRLEN),
	       ipr->ipr_plen, rai->ifname);

	/* free the previous packet */
	free(rai->ra_data);
	rai->ra_data = NULL;

	/* reconstruct the packet */
	make_packet(rai);
}

/*
 * Delete a prefix to the list of specified interface and reconstruct
 * the outgoing packet.
 * The prefix must be in the list.
 */
void
delete_prefix(struct prefix *prefix)
{
	char ntopbuf[INET6_ADDRSTRLEN];
	struct rainfo *rai = prefix->rainfo;

	TAILQ_REMOVE(&rai->prefix, prefix, next);
	rai->pfxs--;
	syslog(LOG_DEBUG, "<%s> prefix %s/%d was deleted on %s",
	       __func__, inet_ntop(AF_INET6, &prefix->prefix,
				       ntopbuf, INET6_ADDRSTRLEN),
	       prefix->prefixlen, rai->ifname);
	rtadvd_remove_timer(&prefix->timer);
	free(prefix);
}

void
invalidate_prefix(struct prefix *prefix)
{
	char ntopbuf[INET6_ADDRSTRLEN];
	struct timespec timo;
	struct rainfo *rai = prefix->rainfo;

	if (prefix->timer) {	/* sanity check */
		syslog(LOG_ERR,
		    "<%s> assumption failure: timer already exists",
		    __func__);
		exit(1);
	}

	syslog(LOG_DEBUG, "<%s> prefix %s/%d was invalidated on %s, "
	    "will expire in %ld seconds", __func__,
	    inet_ntop(AF_INET6, &prefix->prefix, ntopbuf, INET6_ADDRSTRLEN),
	    prefix->prefixlen, rai->ifname, (long)prefix_timo);

	/* set the expiration timer */
	prefix->timer = rtadvd_add_timer(prefix_timeout, NULL, prefix, NULL);
	if (prefix->timer == NULL) {
		syslog(LOG_ERR, "<%s> failed to add a timer for a prefix. "
		    "remove the prefix", __func__);
		delete_prefix(prefix);
	}
	timo.tv_sec = prefix_timo;
	timo.tv_nsec = 0;
	rtadvd_set_timer(&timo, prefix->timer);
}

static struct rtadvd_timer *
prefix_timeout(void *arg)
{
	struct prefix *prefix = (struct prefix *)arg;

	delete_prefix(prefix);

	return(NULL);
}

void
update_prefix(struct prefix * prefix)
{
	char ntopbuf[INET6_ADDRSTRLEN];
	struct rainfo *rai = prefix->rainfo;

	if (prefix->timer == NULL) { /* sanity check */
		syslog(LOG_ERR,
		    "<%s> assumption failure: timer does not exist",
		    __func__);
		exit(1);
	}

	syslog(LOG_DEBUG, "<%s> prefix %s/%d was re-enabled on %s",
	    __func__, inet_ntop(AF_INET6, &prefix->prefix, ntopbuf,
	    INET6_ADDRSTRLEN), prefix->prefixlen, rai->ifname);

	/* stop the expiration timer */
	rtadvd_remove_timer(&prefix->timer);
}

/*
 * Try to get an in6_prefixreq contents for a prefix which matches
 * ipr->ipr_prefix and ipr->ipr_plen and belongs to
 * the interface whose name is ipr->ipr_name[].
 */
static int
init_prefix(struct in6_prefixreq *ipr)
{
#if 0
	int s;

	if ((s = socket(AF_INET6, SOCK_DGRAM, 0)) < 0) {
		syslog(LOG_ERR, "<%s> socket: %m", __func__);
		exit(1);
	}

	if (ioctl(s, SIOCGIFPREFIX_IN6, ipr) < 0) {
		syslog(LOG_INFO, "<%s> ioctl:SIOCGIFPREFIX: %m", __func__);

		ipr->ipr_vltime = DEF_ADVVALIDLIFETIME;
		ipr->ipr_pltime = DEF_ADVPREFERREDLIFETIME;
		ipr->ipr_raf_onlink = 1;
		ipr->ipr_raf_auto = 1;
		/* omit other field initialization */
	}
	else if (ipr->ipr_origin < PR_ORIG_RR) {
		char ntopbuf[INET6_ADDRSTRLEN];

		syslog(LOG_WARNING, "<%s> Added prefix(%s)'s origin %d is"
		       "lower than PR_ORIG_RR(router renumbering)."
		       "This should not happen if I am router", __func__,
		       inet_ntop(AF_INET6, &ipr->ipr_prefix.sin6_addr, ntopbuf,
				 sizeof(ntopbuf)), ipr->ipr_origin);
		close(s);
		return 1;
	}

	close(s);
	return 0;
#else
	ipr->ipr_vltime = DEF_ADVVALIDLIFETIME;
	ipr->ipr_pltime = DEF_ADVPREFERREDLIFETIME;
	ipr->ipr_raf_onlink = 1;
	ipr->ipr_raf_auto = 1;
	return 0;
#endif
}

void
make_prefix(struct rainfo *rai, int ifindex, struct in6_addr *addr, int plen)
{
	struct in6_prefixreq ipr;

	memset(&ipr, 0, sizeof(ipr));
	if (if_indextoname(ifindex, ipr.ipr_name) == NULL) {
		syslog(LOG_ERR, "<%s> Prefix added interface No.%d doesn't"
		       "exist. This should not happen: %m", __func__,
		       ifindex);
		exit(1);
	}
	ipr.ipr_prefix.sin6_len = sizeof(ipr.ipr_prefix);
	ipr.ipr_prefix.sin6_family = AF_INET6;
	ipr.ipr_prefix.sin6_addr = *addr;
	ipr.ipr_plen = plen;

	if (init_prefix(&ipr))
		return; /* init failed by some error */
	add_prefix(rai, &ipr);
}

void
make_packet(struct rainfo *rainfo)
{
	size_t packlen, lladdroptlen = 0;
	char *buf;
	struct nd_router_advert *ra;
	struct nd_opt_prefix_info *ndopt_pi;
	struct nd_opt_mtu *ndopt_mtu;
	struct prefix *pfx;
	struct nd_opt_route_info *ndopt_rti;
	struct rtinfo *rti;
	struct nd_opt_rdnss *ndopt_rdnss;
	struct rdnss *rdns;
	struct rdnss_addr *rdnsa;
	struct nd_opt_dnssl *ndopt_dnssl;
	struct dnssl *dnsl;
	struct dnssl_domain *dnsd;
	size_t len, plen;

	/* calculate total length */
	packlen = sizeof(struct nd_router_advert);
	if (rainfo->advlinkopt) {
		if ((lladdroptlen = lladdropt_length(rainfo->sdl)) == 0) {
			syslog(LOG_INFO,
			       "<%s> link-layer address option has"
			       " null length on %s.  Treat as not included.",
			       __func__, rainfo->ifname);
			rainfo->advlinkopt = 0;
		}
		packlen += lladdroptlen;
	}
	if (TAILQ_FIRST(&rainfo->prefix) != NULL)
		packlen += sizeof(struct nd_opt_prefix_info) * rainfo->pfxs;
	if (rainfo->linkmtu)
		packlen += sizeof(struct nd_opt_mtu);
	TAILQ_FOREACH(rti, &rainfo->route, next) 
		packlen += sizeof(struct nd_opt_route_info) + 
			   ((rti->prefixlen + 0x3f) >> 6) * 8;

	TAILQ_FOREACH(rdns, &rainfo->rdnss, next) {
		packlen += sizeof(struct nd_opt_rdnss);
		TAILQ_FOREACH(rdnsa, &rdns->list, next)
			packlen += sizeof(rdnsa->addr);
	}
	TAILQ_FOREACH(dnsl, &rainfo->dnssl, next) {
		packlen += sizeof(struct nd_opt_dnssl);
		len = 0;
		TAILQ_FOREACH(dnsd, &dnsl->list, next)
			len += dnsd->len;
		len += len % 8 ? 8 - len % 8 : 0;
		packlen += len;
	}

	/* allocate memory for the packet */
	if ((buf = realloc(rainfo->ra_data, packlen)) == NULL) {
		syslog(LOG_ERR,
		       "<%s> can't get enough memory for an RA packet %m",
		       __func__);
		exit(1);
	}
	rainfo->ra_data = buf;
	/* XXX: what if packlen > 576? */
	rainfo->ra_datalen = packlen;
#define CHECKLEN(size) \
	do { \
		if (buf + size > rainfo->ra_data + packlen) { \
			syslog(LOG_ERR, \
			    "<%s, %d> RA packet does not fit in %zu",\
			    __func__, __LINE__, packlen); \
			exit(1); \
		} \
	} while (/*CONSTCOND*/0)
	/*
	 * construct the packet
	 */
	CHECKLEN(sizeof(*ra));
	ra = (struct nd_router_advert *)buf;
	ra->nd_ra_type = ND_ROUTER_ADVERT;
	ra->nd_ra_code = 0;
	ra->nd_ra_cksum = 0;
	ra->nd_ra_curhoplimit = (uint8_t)(0xff & rainfo->hoplimit);
	ra->nd_ra_flags_reserved = 0; /* just in case */
	/*
	 * XXX: the router preference field, which is a 2-bit field, should be
	 * initialized before other fields.
	 */
	ra->nd_ra_flags_reserved = 0xff & rainfo->rtpref;
	ra->nd_ra_flags_reserved |=
		rainfo->managedflg ? ND_RA_FLAG_MANAGED : 0;
	ra->nd_ra_flags_reserved |=
		rainfo->otherflg ? ND_RA_FLAG_OTHER : 0;
	ra->nd_ra_router_lifetime = htons(rainfo->lifetime);
	ra->nd_ra_reachable = htonl(rainfo->reachabletime);
	ra->nd_ra_retransmit = htonl(rainfo->retranstimer);
	buf += sizeof(*ra);

	if (rainfo->advlinkopt) {
		CHECKLEN(sizeof(struct nd_opt_hdr));
		lladdropt_fill(rainfo->sdl, (struct nd_opt_hdr *)buf);
		buf += lladdroptlen;
	}

	if (rainfo->linkmtu) {
		CHECKLEN(sizeof(*ndopt_mtu));
		ndopt_mtu = (struct nd_opt_mtu *)buf;
		ndopt_mtu->nd_opt_mtu_type = ND_OPT_MTU;
		ndopt_mtu->nd_opt_mtu_len = 1;
		ndopt_mtu->nd_opt_mtu_reserved = 0;
		ndopt_mtu->nd_opt_mtu_mtu = htonl(rainfo->linkmtu);
		buf += sizeof(struct nd_opt_mtu);
	}

	TAILQ_FOREACH(pfx, &rainfo->prefix, next) {	
		uint32_t vltime, pltime;
		struct timespec now;

		CHECKLEN(sizeof(*ndopt_pi));
		ndopt_pi = (struct nd_opt_prefix_info *)buf;
		ndopt_pi->nd_opt_pi_type = ND_OPT_PREFIX_INFORMATION;
		ndopt_pi->nd_opt_pi_len = 4;
		ndopt_pi->nd_opt_pi_prefix_len = pfx->prefixlen;
		ndopt_pi->nd_opt_pi_flags_reserved = 0;
		if (pfx->onlinkflg)
			ndopt_pi->nd_opt_pi_flags_reserved |=
				ND_OPT_PI_FLAG_ONLINK;
		if (pfx->autoconfflg)
			ndopt_pi->nd_opt_pi_flags_reserved |=
				ND_OPT_PI_FLAG_AUTO;
		if (pfx->timer)
			vltime = 0;
		else {
			if (pfx->vltimeexpire || pfx->pltimeexpire)
				clock_gettime(CLOCK_MONOTONIC, &now);
			if (pfx->vltimeexpire == 0)
				vltime = pfx->validlifetime;
			else
				vltime = (pfx->vltimeexpire > now.tv_sec) ?
				    pfx->vltimeexpire - now.tv_sec : 0;
		}
		if (pfx->timer)
			pltime = 0;
		else {
			if (pfx->pltimeexpire == 0)
				pltime = pfx->preflifetime;
			else
				pltime = (pfx->pltimeexpire > now.tv_sec) ? 
				    pfx->pltimeexpire - now.tv_sec : 0;
		}
		if (vltime < pltime) {
			/*
			 * this can happen if vltime is decrement but pltime
			 * is not.
			 */
			pltime = vltime;
		}
		ndopt_pi->nd_opt_pi_valid_time = htonl(vltime);
		ndopt_pi->nd_opt_pi_preferred_time = htonl(pltime);
		ndopt_pi->nd_opt_pi_reserved2 = 0;
		ndopt_pi->nd_opt_pi_prefix = pfx->prefix;

		buf += sizeof(struct nd_opt_prefix_info);
	}

	TAILQ_FOREACH(rti, &rainfo->route, next) {
		uint8_t psize = (rti->prefixlen + 0x3f) >> 6;

		CHECKLEN(sizeof(*ndopt_rti));
		ndopt_rti = (struct nd_opt_route_info *)buf;
		ndopt_rti->nd_opt_rti_type = ND_OPT_ROUTE_INFO;
		ndopt_rti->nd_opt_rti_len = 1 + psize;
		ndopt_rti->nd_opt_rti_prefixlen = rti->prefixlen;
		ndopt_rti->nd_opt_rti_flags = 0xff & rti->rtpref;
		ndopt_rti->nd_opt_rti_lifetime = htonl(rti->ltime);
		memcpy(ndopt_rti + 1, &rti->prefix, psize * 8);
		buf += sizeof(struct nd_opt_route_info) + psize * 8;
	}

	TAILQ_FOREACH(rdns, &rainfo->rdnss, next) {
		CHECKLEN(sizeof(*ndopt_rdnss));
		ndopt_rdnss = (struct nd_opt_rdnss *)buf;
		ndopt_rdnss->nd_opt_rdnss_type = ND_OPT_RDNSS;
		ndopt_rdnss->nd_opt_rdnss_len = 1;
		ndopt_rdnss->nd_opt_rdnss_reserved = 0;
		ndopt_rdnss->nd_opt_rdnss_lifetime = htonl(rdns->lifetime);
		buf += sizeof(*ndopt_rdnss);
	
		TAILQ_FOREACH(rdnsa, &rdns->list, next) {
			CHECKLEN(sizeof(rdnsa->addr));
			memcpy(buf, &rdnsa->addr, sizeof(rdnsa->addr));
			ndopt_rdnss->nd_opt_rdnss_len += 2;
			buf += sizeof(rdnsa->addr);
		}
	}

	TAILQ_FOREACH(dnsl, &rainfo->dnssl, next) {
		CHECKLEN(sizeof(*ndopt_dnssl));
		ndopt_dnssl = (struct nd_opt_dnssl *)buf;
		ndopt_dnssl->nd_opt_dnssl_type = ND_OPT_DNSSL;
		ndopt_dnssl->nd_opt_dnssl_len = 0;
		ndopt_dnssl->nd_opt_dnssl_reserved = 0;
		ndopt_dnssl->nd_opt_dnssl_lifetime = htonl(dnsl->lifetime);
		buf += sizeof(*ndopt_dnssl);
	
		TAILQ_FOREACH(dnsd, &dnsl->list, next) {
			CHECKLEN(dnsd->len);
			memcpy(buf, dnsd->domain, dnsd->len);
			buf += dnsd->len;
		}
		/* Ensure our length is padded correctly */
		len = buf - (char *)ndopt_dnssl;
		plen = len % 8 ? 8 - len % 8 : 0;
		CHECKLEN(plen);
		memset(buf, 0, plen);
		buf += plen;
		ndopt_dnssl->nd_opt_dnssl_len = (len + plen) / 8;
	}
	memset(buf, 0, packlen - (buf - rainfo->ra_data));
}

static int
getinet6sysctl(int code)
{
	const int mib[] = { CTL_NET, PF_INET6, IPPROTO_IPV6, code };
	int value;
	size_t size;

	size = sizeof(value);
	if (sysctl(mib, __arraycount(mib), &value, &size, NULL, 0)
	    < 0) {
		syslog(LOG_ERR, "<%s>: failed to get ip6 sysctl(%d): %m",
		       __func__, code);
		return -1;
	}
	else
		return value;
}
