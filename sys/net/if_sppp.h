/*	$NetBSD: if_sppp.h,v 1.28 2015/09/06 06:01:01 dholland Exp $	*/

/*-
 * Copyright (c) 2002 The NetBSD Foundation, Inc.
 * All rights reserved.
 *
 * This code is derived from software contributed to The NetBSD Foundation
 * by Martin Husemann <martin@NetBSD.org>.
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

#ifndef _NET_IF_SPPP_H_
#define _NET_IF_SPPP_H_

/* ioctls used by the if_spppsubr.c driver */

#include <sys/ioccom.h>


#define	SPPP_AUTHPROTO_NONE	0
#define SPPP_AUTHPROTO_PAP	1
#define SPPP_AUTHPROTO_CHAP	2

#define SPPP_AUTHFLAG_NOCALLOUT		1	/* do not require authentication on */
						/* callouts */
#define SPPP_AUTHFLAG_NORECHALLENGE	2	/* do not re-challenge CHAP */

struct spppauthcfg {
	char	ifname[IFNAMSIZ];	/* pppoe interface name */
	u_int	hisauth;		/* one of SPPP_AUTHPROTO_* above */
	u_int	myauth;			/* one of SPPP_AUTHPROTO_* above */
	u_int	myname_length;		/* includes terminating 0 */
	u_int	mysecret_length;	/* includes terminating 0 */
	u_int	hisname_length;		/* includes terminating 0 */
	u_int	hissecret_length;	/* includes terminating 0 */
	u_int	myauthflags;
	u_int	hisauthflags;
	char	*myname;
	char	*mysecret;
	char	*hisname;
	char	*hissecret;
};

#define	SPPPGETAUTHCFG	_IOWR('i', 120, struct spppauthcfg)
#define	SPPPSETAUTHCFG	_IOW('i', 121, struct spppauthcfg)

struct sppplcpcfg {
	char	ifname[IFNAMSIZ];	/* pppoe interface name */
	int	lcp_timeout;		/* LCP timeout, in ticks */
};

#define	SPPPGETLCPCFG	_IOWR('i', 122, struct sppplcpcfg)
#define	SPPPSETLCPCFG	_IOW('i', 123, struct sppplcpcfg)

/*
 * Don't change the order of this.  Ordering the phases this way allows
 * for a comparision of ``pp_phase >= PHASE_AUTHENTICATE'' in order to
 * know whether LCP is up.
 */
#define	SPPP_PHASE_DEAD		0
#define	SPPP_PHASE_ESTABLISH	1
#define	SPPP_PHASE_TERMINATE	2
#define	SPPP_PHASE_AUTHENTICATE	3
#define	SPPP_PHASE_NETWORK	4

struct spppstatus {
	char	ifname[IFNAMSIZ];	/* pppoe interface name */
	int	phase;			/* one of SPPP_PHASE_* above */
};

#define	SPPPGETSTATUS	_IOWR('i', 124, struct spppstatus)

struct spppstatusncp {
	char	ifname[IFNAMSIZ];	/* pppoe interface name */
	int	phase;			/* one of SPPP_PHASE_* above */
	int	ncpup;			/* != 0 if at least on NCP is up */
};

#define	SPPPGETSTATUSNCP	_IOWR('i', 134, struct spppstatusncp)

struct spppidletimeout {
	char	ifname[IFNAMSIZ];	/* pppoe interface name */
	time_t	idle_seconds;		/* number of seconds idle before
					 * disconnect, 0 to disable idle-timeout */
};

struct spppidletimeout50 {
	char	ifname[IFNAMSIZ];	/* pppoe interface name */
	uint32_t idle_seconds;		/* number of seconds idle before
					 * disconnect, 0 to disable idle-timeout */
};

#define	SPPPGETIDLETO	_IOWR('i', 125, struct spppidletimeout)
#define	SPPPSETIDLETO	_IOW('i', 126, struct spppidletimeout)
#define	__SPPPGETIDLETO50	_IOWR('i', 125, struct spppidletimeout50)
#define	__SPPPSETIDLETO50	_IOW('i', 126, struct spppidletimeout50)

struct spppauthfailurestats {
	char	ifname[IFNAMSIZ];	/* pppoe interface name */
	int	auth_failures;		/* number of LCP failures since last successful TLU */
	int	max_failures;		/* max. allowed authorization failures */
};

#define	SPPPGETAUTHFAILURES	_IOWR('i', 127, struct spppauthfailurestats)

struct spppauthfailuresettings {
	char	ifname[IFNAMSIZ];	/* pppoe interface name */
	int	max_failures;		/* max. allowed authorization failures */
};
#define	SPPPSETAUTHFAILURE	_IOW('i', 128, struct spppauthfailuresettings)

/* set the DNS options we would like to query during PPP negotiation */
struct spppdnssettings {
	char	ifname[IFNAMSIZ];	/* pppoe interface name */
	int	query_dns;		/* bitmask (bits 0 and 1) for DNS options to query in IPCP */
};
#define	SPPPSETDNSOPTS		_IOW('i', 129, struct spppdnssettings)
#define	SPPPGETDNSOPTS		_IOWR('i', 130, struct spppdnssettings)

/* get the DNS addresses we received from the peer */
struct spppdnsaddrs {
	char	ifname[IFNAMSIZ];	/* pppoe interface name */
	uint32_t dns[2];		/* IP addresses */
};

#define SPPPGETDNSADDRS		_IOWR('i', 131, struct spppdnsaddrs)

/* set LCP keepalive/timeout options */
struct spppkeepalivesettings {
	char	ifname[IFNAMSIZ];	/* pppoe interface name */
	u_int	maxalive;		/* number of LCP echo req. w/o reply */
	time_t	max_noreceive;		/* (sec.) grace period before we start
					   sending LCP echo requests. */
};
struct spppkeepalivesettings50 {
	char	ifname[IFNAMSIZ];	/* pppoe interface name */
	u_int	maxalive;		/* number of LCP echo req. w/o reply */
	uint32_t max_noreceive;		/* (sec.) grace period before we start
					   sending LCP echo requests. */
};
#define	SPPPSETKEEPALIVE	_IOW('i', 132, struct spppkeepalivesettings)
#define	SPPPGETKEEPALIVE	_IOWR('i', 133, struct spppkeepalivesettings)
#define	__SPPPSETKEEPALIVE50	_IOW('i', 132, struct spppkeepalivesettings50)
#define	__SPPPGETKEEPALIVE50	_IOWR('i', 133, struct spppkeepalivesettings50)

/* 134 already used! */

#endif /* !_NET_IF_SPPP_H_ */

