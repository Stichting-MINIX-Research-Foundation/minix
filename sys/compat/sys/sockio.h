/*	$NetBSD: sockio.h,v 1.10 2010/11/14 15:36:47 uebayasi Exp $	*/

/*-
 * Copyright (c) 1982, 1986, 1990, 1993, 1994
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

#ifndef _COMPAT_SYS_SOCKIO_H_
#define	_COMPAT_SYS_SOCKIO_H_

#ifdef _KERNEL_OPT

#include "opt_compat_netbsd.h"
#include "opt_modular.h"

#include <sys/ioccom.h>

#if defined(COMPAT_09) || defined(COMPAT_10) || defined(COMPAT_11) || \
    defined(COMPAT_12) || defined(COMPAT_13) || defined(COMPAT_14) || \
    defined(COMPAT_15) || defined(COMPAT_16) || defined(COMPAT_20) || \
    defined(COMPAT_30) || defined(COMPAT_40) || defined(MODULAR)
#define COMPAT_OIFREQ
#endif

#if defined(COMPAT_09) || defined(COMPAT_10) || defined(COMPAT_11) || \
    defined(COMPAT_12) || defined(COMPAT_13) || defined(COMPAT_14) || \
    defined(COMPAT_15) || defined(COMPAT_16) || defined(COMPAT_20) || \
    defined(COMPAT_30) || defined(COMPAT_40) || defined(COMPAT_50) || \
    defined(MODULAR)
#define COMPAT_OIFDATA
#endif

#endif /* _KERNEL_OPT */

struct oifreq {
	char	ifr_name[IFNAMSIZ];		/* if name, e.g. "en0" */
	union {
		struct	sockaddr ifru_addr;
		struct	sockaddr ifru_dstaddr;
		struct	sockaddr ifru_broadaddr;
		short	ifru_flags;
		int	ifru_metric;
		int	ifru_mtu;
		int	ifru_dlt;
		u_int	ifru_value;
		void *	ifru_data;
		struct {
			uint32_t	b_buflen;
			void		*b_buf;
		} ifru_b;
	} ifr_ifru;
};
struct	oifconf {
	int	ifc_len;		/* size of associated buffer */
	union {
		void *	ifcu_buf;
		struct	oifreq *ifcu_req;
	} ifc_ifcu;
#define	ifc_buf	ifc_ifcu.ifcu_buf	/* buffer address */
#define	ifc_req	ifc_ifcu.ifcu_req	/* array of structures returned */
};

#include <compat/sys/time.h>
/*
 * Structure defining statistics and other data kept regarding a network
 * interface.
 */
struct oif_data {
	/* generic interface information */
	u_char	ifi_type;		/* ethernet, tokenring, etc. */
	u_char	ifi_addrlen;		/* media address length */
	u_char	ifi_hdrlen;		/* media header length */
	int	ifi_link_state;		/* current link state */
	u_quad_t ifi_mtu;		/* maximum transmission unit */
	u_quad_t ifi_metric;		/* routing metric (external only) */
	u_quad_t ifi_baudrate;		/* linespeed */
	/* volatile statistics */
	u_quad_t ifi_ipackets;		/* packets received on interface */
	u_quad_t ifi_ierrors;		/* input errors on interface */
	u_quad_t ifi_opackets;		/* packets sent on interface */
	u_quad_t ifi_oerrors;		/* output errors on interface */
	u_quad_t ifi_collisions;	/* collisions on csma interfaces */
	u_quad_t ifi_ibytes;		/* total number of octets received */
	u_quad_t ifi_obytes;		/* total number of octets sent */
	u_quad_t ifi_imcasts;		/* packets received via multicast */
	u_quad_t ifi_omcasts;		/* packets sent via multicast */
	u_quad_t ifi_iqdrops;		/* dropped on input, this interface */
	u_quad_t ifi_noproto;		/* destined for unsupported protocol */
	struct	timeval50 ifi_lastchange;/* last operational state change */
};

struct oifdatareq {
	char	ifdr_name[IFNAMSIZ];		/* if name, e.g. "en0" */
	struct	oif_data ifdr_data;
};

#define	OSIOCSIFADDR	 _IOW('i', 12, struct oifreq)	/* set ifnet address */
#define	OOSIOCGIFADDR	 _IOWR('i', 13, struct oifreq)	/* get ifnet address */
#define	OSIOCSIFDSTADDR	 _IOW('i', 14, struct oifreq)	/* set p-p address */
#define	OOSIOCGIFDSTADDR _IOWR('i', 15, struct oifreq)	/* get p-p address */
#define OSIOCSIFFLAGS    _IOW('i', 16, struct oifreq)	/* set ifnet flags */
#define	OSIOCGIFFLAGS    _IOWR('i', 17, struct oifreq)	/* get ifnet flags */
#define	OOSIOCGIFBRDADDR _IOWR('i', 18, struct oifreq)	/* get bcast addr */
#define	OSIOCSIFBRDADDR	 _IOW('i', 19, struct oifreq)	/* set bcast addr */
#define	OOSIOCGIFCONF	 _IOWR('i', 20, struct ifconf)	/* get ifnet list */
#define	OOSIOCGIFNETMASK _IOWR('i', 21, struct oifreq)	/* get net addr mask */
#define	OSIOCSIFNETMASK	 _IOW('i', 22, struct oifreq)	/* set net addr mask */
#define	OSIOCGIFCONF	 _IOWR('i', 36, struct ifconf)	/* get ifnet list */
#define	OSIOCADDMULTI	 _IOW('i', 49, struct oifreq)	/* add m'cast addr */
#define	OSIOCDELMULTI	 _IOW('i', 50, struct oifreq)	/* del m'cast addr */
#define	OSIOCSIFMEDIA	 _IOWR('i', 53, struct oifreq)	/* set net media */
#define	OSIOCGIFMTU	 _IOWR('i', 126, struct oifreq)	/* get ifnet mtu */
#define	OSIOCGIFDATA	 _IOWR('i', 128, struct oifdatareq) /* get if_data */
#define	OSIOCZIFDATA	 _IOWR('i', 129, struct oifdatareq) /* get if_data then
							     zero ctrs*/




#define	OBIOCGETIF	 _IOR('B', 107, struct oifreq)
#define	OBIOCSETIF	 _IOW('B', 108, struct oifreq)
#define	OTAPGIFNAME	 _IOR('e', 0, struct oifreq)

#define ifreqn2o(oi, ni) \
	do { \
		(void)memcpy((oi)->ifr_name, (ni)->ifr_name, \
		    sizeof((oi)->ifr_name)); \
		(void)memcpy(&(oi)->ifr_ifru, &(ni)->ifr_ifru, \
		    sizeof((oi)->ifr_ifru)); \
	} while (/*CONSTCOND*/0)

#define ifreqo2n(oi, ni) \
	do { \
		(void)memcpy((ni)->ifr_name, (oi)->ifr_name, \
		    sizeof((oi)->ifr_name)); \
		(void)memcpy(&(ni)->ifr_ifru, &(oi)->ifr_ifru, \
		    sizeof((oi)->ifr_ifru)); \
	} while (/*CONSTCOND*/0)

#define ifdatan2o(oi, ni) \
	do { \
		(void)memcpy((oi), (ni),  sizeof(*(oi))); \
		(oi)->ifi_lastchange.tv_sec = \
		    (int32_t)(ni)->ifi_lastchange.tv_sec; \
		(oi)->ifi_lastchange.tv_usec = \
		    (ni)->ifi_lastchange.tv_nsec / 1000; \
	} while (/*CONSTCOND*/0)

#define ifdatao2n(oi, ni) \
	do { \
		(void)memcpy((ni), (oi),  sizeof(*(oi))); \
		    sizeof((oi)->ifr_name)); \
		(ni)->ifi_lastchange.tv_sec = (oi)->ifi_lastchange.tv_sec; \
		(ni)->ifi_lastchange.tv_nsec = \
		    (oi)->ifi_lastchange.tv_usec * 1000; \
	} while (/*CONSTCOND*/0)
#ifdef _KERNEL
__BEGIN_DECLS
int compat_ifconf(u_long, void *);
int compat_ifdatareq(struct lwp *, u_long, void *);
__END_DECLS
#endif
#endif /* _COMPAT_SYS_SOCKIO_H_ */
