/*	$NetBSD: rtadvd.h,v 1.14 2015/06/05 14:09:20 roy Exp $	*/
/*	$KAME: rtadvd.h,v 1.30 2005/10/17 14:40:02 suz Exp $	*/

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

#define RTADVD_USER	"_rtadvd"

#define ALLNODES "ff02::1"
#define ALLROUTERS_LINK "ff02::2"
#define ALLROUTERS_SITE "ff05::2"

#define IN6ADDR_SITELOCAL_ALLROUTERS_INIT \
	{{{ 0xff, 0x05, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, \
 	    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x02 }}}

//extern struct sockaddr_in6 sin6_linklocal_allnodes;
//extern struct sockaddr_in6 sin6_linklocal_allrouters;
extern struct sockaddr_in6 sin6_sitelocal_allrouters;

/* protocol constants and default values */
#define DEF_MAXRTRADVINTERVAL 600
#define DEF_ADVLINKMTU 0
#define DEF_ADVREACHABLETIME 0
#define DEF_ADVRETRANSTIMER 0
#define DEF_ADVCURHOPLIMIT 64
#define DEF_ADVVALIDLIFETIME 2592000
#define DEF_ADVPREFERREDLIFETIME 604800

#define MAXROUTERLIFETIME 9000
#define MIN_MAXINTERVAL 4
#define MAX_MAXINTERVAL 1800
#define MIN_MININTERVAL 3
#define MAXREACHABLETIME 3600000

#define MAX_INITIAL_RTR_ADVERT_INTERVAL  16
#define MAX_INITIAL_RTR_ADVERTISEMENTS    3
#define MAX_FINAL_RTR_ADVERTISEMENTS      3
#define MIN_DELAY_BETWEEN_RAS             3
#define MAX_RA_DELAY_TIME                500000000 /* nsec */

#define PREFIX_FROM_KERNEL 1
#define PREFIX_FROM_CONFIG 2
#define PREFIX_FROM_DYNAMIC 3

struct prefix {
	TAILQ_ENTRY(prefix) next;

	struct rainfo *rainfo;	/* back pointer to the interface */

	struct rtadvd_timer *timer; /* expiration timer.  used when a prefix
				     * derived from the kernel is deleted.
				     */

	uint32_t validlifetime; /* AdvValidLifetime */
	long	vltimeexpire;	/* expiration of vltime; decrement case only */
	uint32_t preflifetime;	/* AdvPreferredLifetime */
	long	pltimeexpire;	/* expiration of pltime; decrement case only */
	uint16_t onlinkflg;	/* bool: AdvOnLinkFlag */
	uint16_t autoconfflg;	/* bool: AdvAutonomousFlag */
	int prefixlen;
	int origin;		/* from kernel or config */
	struct in6_addr prefix;
};

struct rtinfo {
	TAILQ_ENTRY(rtinfo) next;

	uint32_t ltime;	/* route lifetime */
	uint16_t rtpref;		/* route preference */
	int prefixlen;
	struct in6_addr prefix;
};

struct rdnss_addr {
	TAILQ_ENTRY(rdnss_addr) next;

	struct in6_addr addr;
};

struct rdnss {
	TAILQ_ENTRY(rdnss) next;

	TAILQ_HEAD(, rdnss_addr) list;
	uint32_t lifetime;
};

struct dnssl_domain {
	TAILQ_ENTRY(dnssl_domain) next;

	int len;
	char domain[256];
};

struct dnssl {
	TAILQ_ENTRY(dnssl) next;

	TAILQ_HEAD(, dnssl_domain) list;
	uint32_t lifetime;
};

struct soliciter {
	TAILQ_ENTRY(soliciter) next;

	struct sockaddr_in6 addr;
};

struct	rainfo {
	TAILQ_ENTRY(rainfo) next;

	/* timer related parameters */
	struct rtadvd_timer *timer;
	int initcounter; /* counter for the first few advertisements */
	struct timespec lastsent; /* timestamp when the latest RA was sent */
	int waiting;		/* number of RS waiting for RA */
	struct rainfo *leaving;		/* the config which is leaving */
	struct rainfo *leaving_for;	/* the new config to activate */
	int leaving_adv;		/* number of RA left to send */

	/* interface information */
	uint16_t	ifindex;
	int		ifflags;
	int	advlinkopt;	/* bool: whether include link-layer addr opt */
	struct sockaddr_dl *sdl;
	char	ifname[16];
	uint32_t	phymtu;		/* mtu of the physical interface */

	/* Router configuration variables */
	uint16_t	lifetime;	/* AdvDefaultLifetime */
	uint16_t	maxinterval;	/* MaxRtrAdvInterval */
	uint16_t	mininterval;	/* MinRtrAdvInterval */
	int 	managedflg;	/* AdvManagedFlag */
	int	otherflg;	/* AdvOtherConfigFlag */

	int	rtpref;		/* router preference */
	uint32_t linkmtu;	/* AdvLinkMTU */
	uint32_t reachabletime; /* AdvReachableTime */
	uint32_t retranstimer;	/* AdvRetransTimer */
	uint16_t	hoplimit;	/* AdvCurHopLimit */
	TAILQ_HEAD(, prefix) prefix;	/* AdvPrefixList(link head) */
	int	pfxs;
	uint16_t	clockskew;/* used for consisitency check of lifetimes */

	TAILQ_HEAD(, rtinfo) route;
	TAILQ_HEAD(, rdnss) rdnss;	/* RDNSS list */
	TAILQ_HEAD(, dnssl) dnssl;	/* DNS Search List */

	/* actual RA packet data and its length */
	size_t ra_datalen;
	char *ra_data;

	/* statistics */
	uint64_t raoutput;	/* number of RAs sent */
	uint64_t rainput;	/* number of RAs received */
	uint64_t rainconsistent; /* number of RAs inconsistent with ours */
	uint64_t rsinput;	/* number of RSs received */

	/* info about soliciter */
	TAILQ_HEAD(, soliciter) soliciter;	/* recent solication source */
};

extern TAILQ_HEAD(ralist_head_t, rainfo) ralist;

struct rtadvd_timer *ra_timeout(void *);
void ra_timer_update(void *, struct timespec *);
void ra_timer_set_short_delay(struct rainfo *);

int prefix_match(struct in6_addr *, int, struct in6_addr *, int);
struct rainfo *if_indextorainfo(unsigned int);
struct prefix *find_prefix(struct rainfo *, struct in6_addr *, int);
