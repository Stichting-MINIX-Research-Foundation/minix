/*	$NetBSD: socket.h,v 1.107 2012/06/22 18:26:35 christos Exp $	*/

/*
 * Copyright (C) 1995, 1996, 1997, and 1998 WIDE Project.
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

/*
 * Copyright (c) 1982, 1985, 1986, 1988, 1993, 1994
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
 *
 *	@(#)socket.h	8.6 (Berkeley) 5/3/95
 */

#ifndef _SYS_SOCKET_H_
#define	_SYS_SOCKET_H_

#include <sys/featuretest.h>

/*
 * Definitions related to sockets: types, address families, options.
 */

/*
 * Data types.
 */
#include <sys/ansi.h>

#ifndef sa_family_t
typedef __sa_family_t	sa_family_t;
#define sa_family_t	__sa_family_t
#endif

#ifndef socklen_t
typedef __socklen_t	socklen_t;
#define socklen_t	__socklen_t
#endif

#include <machine/ansi.h>

#ifdef	_BSD_SIZE_T_
typedef	_BSD_SIZE_T_	size_t;
#undef	_BSD_SIZE_T_
#endif

#ifdef	_BSD_SSIZE_T_
typedef	_BSD_SSIZE_T_	ssize_t;
#undef	_BSD_SSIZE_T_
#endif

#include <sys/uio.h>
#include <sys/sigtypes.h>

/*
 * Socket types.
 */
#define	SOCK_STREAM	1		/* stream socket */
#define	SOCK_DGRAM	2		/* datagram socket */
#define	SOCK_RAW	3		/* raw-protocol interface */
#define	SOCK_RDM	4		/* reliably-delivered message */
#define	SOCK_SEQPACKET	5		/* sequenced packet stream */

#define	SOCK_CLOEXEC	0x10000000	/* set close on exec on socket */
#define	SOCK_NONBLOCK	0x20000000	/* set non blocking i/o socket */
#define	SOCK_NOSIGPIPE	0x40000000	/* don't send sigpipe */
#define	SOCK_FLAGS_MASK	0xf0000000	/* flags mask */

/*
 * Option flags per-socket.
 */
#define	SO_DEBUG	0x0001		/* turn on debugging info recording */
#define	SO_ACCEPTCONN	0x0002		/* socket has had listen() */
#define	SO_REUSEADDR	0x0004		/* allow local address reuse */
#define	SO_KEEPALIVE	0x0008		/* keep connections alive */
#define	SO_TYPE		0x0010		/* get socket type */

#define SO_PASSCRED	0x0012
#define SO_PEERCRED	0x0014

/*
 * Additional options, not kept in so_options.
 */
#define SO_SNDBUF	0x1001		/* send buffer size */
#define SO_RCVBUF	0x1002		/* receive buffer size */
#define SO_SNDLOWAT	0x1003		/* send low-water mark */
#define SO_RCVLOWAT	0x1004		/* receive low-water mark */
/* SO_OSNDTIMEO		0x1005 */
/* SO_ORCVTIMEO		0x1006 */
#define	SO_ERROR	0x1007		/* get error status and clear */
#define	SO_OVERFLOWED	0x1009		/* datagrams: return packets dropped */


/*
 * Level number for (get/set)sockopt() to apply to socket itself.
 */
#define	SOL_SOCKET	0xffff		/* options for socket level */

/*
 * Address families.
 */
#define	AF_UNSPEC	0		/* unspecified */
#define	AF_LOCAL	1		/* local to host */
#define	AF_UNIX		AF_LOCAL	/* backward compatibility */
#define	AF_INET		2		/* internetwork: UDP, TCP, etc. */
#define	AF_IMPLINK	3		/* arpanet imp addresses */
#define	AF_PUP		4		/* pup protocols: e.g. BSP */
#define	AF_CHAOS	5		/* mit CHAOS protocols */
#define	AF_NS		6		/* XEROX NS protocols */
#define	AF_ISO		7		/* ISO protocols */
#define	AF_OSI		AF_ISO
#define	AF_ECMA		8		/* european computer manufacturers */
#define	AF_DATAKIT	9		/* datakit protocols */
#define	AF_CCITT	10		/* CCITT protocols, X.25 etc */
#define	AF_SNA		11		/* IBM SNA */
#define AF_DECnet	12		/* DECnet */
#define AF_DLI		13		/* DEC Direct data link interface */
#define AF_LAT		14		/* LAT */
#define	AF_HYLINK	15		/* NSC Hyperchannel */
#define	AF_APPLETALK	16		/* Apple Talk */
#define	AF_ROUTE	17		/* Internal Routing Protocol */
#define	AF_LINK		18		/* Link layer interface */
#if defined(_NETBSD_SOURCE)
#define	pseudo_AF_XTP	19		/* eXpress Transfer Protocol (no AF) */
#endif
#define	AF_COIP		20		/* connection-oriented IP, aka ST II */
#define	AF_CNT		21		/* Computer Network Technology */
#if defined(_NETBSD_SOURCE)
#define pseudo_AF_RTIP	22		/* Help Identify RTIP packets */
#endif
#define	AF_IPX		23		/* Novell Internet Protocol */
#define	AF_INET6	24		/* IP version 6 */
#if defined(_NETBSD_SOURCE)
#define pseudo_AF_PIP	25		/* Help Identify PIP packets */
#endif
#define AF_ISDN		26		/* Integrated Services Digital Network*/
#define AF_E164		AF_ISDN		/* CCITT E.164 recommendation */
#define AF_NATM		27		/* native ATM access */
#define AF_ARP		28		/* (rev.) addr. res. prot. (RFC 826) */
#if defined(_NETBSD_SOURCE)
#define pseudo_AF_KEY	29		/* Internal key management protocol  */
#define	pseudo_AF_HDRCMPLT 30		/* Used by BPF to not rewrite hdrs
					   in interface output routine */
#endif
#define AF_BLUETOOTH	31		/* Bluetooth: HCI, SCO, L2CAP, RFCOMM */
#define	AF_IEEE80211	32		/* IEEE80211 */

#define	AF_MAX		33

#ifndef	gid_t
typedef	__gid_t		gid_t;		/* group id */
#define	gid_t		__gid_t
#endif

#ifndef	uid_t
typedef	__uid_t		uid_t;		/* user id */
#define	uid_t		__uid_t
#endif

#if defined(__minix) && !defined(_STANDALONE)
#include <sys/ucred.h>
#endif

/*
 * Structure used by kernel to store most
 * addresses.
 */
struct sockaddr
{
	sa_family_t	sa_family;
	char		sa_data[8];	/* Big enough for sockaddr_in */
};

/*
 * RFC 2553: protocol-independent placeholder for socket addresses
 */
#define _SS_MAXSIZE	128
#define _SS_ALIGNSIZE	(sizeof(__int64_t))
#define _SS_PAD1SIZE	(_SS_ALIGNSIZE - 1)
#define _SS_PAD2SIZE	(_SS_MAXSIZE - 1 - \
				_SS_PAD1SIZE - _SS_ALIGNSIZE)

#if (_XOPEN_SOURCE - 0) >= 500 || defined(_NETBSD_SOURCE)
struct sockaddr_storage {
	sa_family_t	ss_family;	/* address family */
	char		__ss_pad1[_SS_PAD1SIZE];
	__int64_t     __ss_align;/* force desired structure storage alignment */
	char		__ss_pad2[_SS_PAD2SIZE];
};
#define	sstosa(__ss)	((struct sockaddr *)(__ss))
#define	sstocsa(__ss)	((const struct sockaddr *)(__ss))
#endif /* _XOPEN_SOURCE >= 500 || _NETBSD_SOURCE */

/*
 * Protocol families, same as address families for now.
 */
#define	PF_UNSPEC	AF_UNSPEC
#define	PF_LOCAL	AF_LOCAL
#define	PF_UNIX		PF_LOCAL	/* backward compatibility */
#define PF_FILE		PF_LOCAL	/* Minix compatibility */
#define	PF_INET		AF_INET
#define	PF_IMPLINK	AF_IMPLINK
#define	PF_PUP		AF_PUP
#define	PF_CHAOS	AF_CHAOS
#define	PF_NS		AF_NS
#define	PF_ISO		AF_ISO
#define	PF_OSI		AF_ISO
#define	PF_ECMA		AF_ECMA
#define	PF_DATAKIT	AF_DATAKIT
#define	PF_CCITT	AF_CCITT
#define	PF_SNA		AF_SNA
#define PF_DECnet	AF_DECnet
#define PF_DLI		AF_DLI
#define PF_LAT		AF_LAT
#define	PF_HYLINK	AF_HYLINK
#define	PF_APPLETALK	AF_APPLETALK
#define	PF_OROUTE	AF_OROUTE
#define	PF_LINK		AF_LINK
#if defined(_NETBSD_SOURCE)
#define	PF_XTP		pseudo_AF_XTP	/* really just proto family, no AF */
#endif
#define	PF_COIP		AF_COIP
#define	PF_CNT		AF_CNT
#define	PF_INET6	AF_INET6
#define	PF_IPX		AF_IPX		/* same format as AF_NS */
#if defined(_NETBSD_SOURCE)
#define PF_RTIP		pseudo_AF_RTIP	/* same format as AF_INET */
#define PF_PIP		pseudo_AF_PIP
#endif
#define PF_ISDN		AF_ISDN		/* same as E164 */
#define PF_E164		AF_E164
#define PF_NATM		AF_NATM
#define PF_ARP		AF_ARP
#if defined(_NETBSD_SOURCE)
#define PF_KEY 		pseudo_AF_KEY	/* like PF_ROUTE, only for key mgmt */
#endif
#define PF_BLUETOOTH	AF_BLUETOOTH
#define	PF_MPLS		AF_MPLS
#define	PF_ROUTE	AF_ROUTE

#define	PF_MAX		AF_MAX

/*
 * Message header for recvmsg and sendmsg calls.
 * Used value-result for recvmsg, value only for sendmsg.
 */
struct msghdr {
	void		*msg_name;	/* optional address */
	socklen_t	msg_namelen;	/* size of address */
	struct iovec	*msg_iov;	/* scatter/gather array */
	int		msg_iovlen;	/* # elements in msg_iov */
	void		*msg_control;	/* ancillary data, see below */
	socklen_t	msg_controllen;	/* ancillary data buffer len */
	int		msg_flags;	/* flags on received message */
};

#define MSG_OOB         0x0001  /* process out-of-band data */
#define MSG_PEEK        0x0002  /* peek at incoming message */
#define MSG_DONTROUTE   0x0004  /* send without using routing tables */
#define MSG_EOR         0x0008         /* complete record */
/*
 * Header for ancillary data objects in msg_control buffer.
 * Used for additional information with/about a datagram
 * not expressible by flags.  The format is a sequence
 * of message elements headed by cmsghdr structures.
 */
struct cmsghdr {
	socklen_t	cmsg_len;	/* data byte count, including hdr */
	int		cmsg_level;	/* originating protocol */
	int		cmsg_type;	/* protocol-specific type */
/* followed by	u_char  cmsg_data[]; */
};

#define CMSG_FIRSTHDR(mhdr) 					\
	( (mhdr)->msg_controllen >= sizeof(struct cmsghdr) ? 	\
		(struct cmsghdr *)(mhdr)->msg_control : 	\
		(struct cmsghdr *)NULL )

#define CMSG_ALIGN(len)						\
	( (len % sizeof(long) == 0) ?				\
		len :						\
		len + sizeof(long) - (len  % sizeof(long)) )

#define CMSG_NXTHDR(mhdr, cmsg) 					\
	( ((cmsg) == NULL) ? CMSG_FIRSTHDR(mhdr) : 			\
		(((unsigned char *)(cmsg) + CMSG_ALIGN((cmsg)->cmsg_len) \
		+ CMSG_ALIGN(sizeof(struct cmsghdr)) >			\
		(unsigned char *)((mhdr)->msg_control) +		\
		(mhdr)->msg_controllen) ? 				\
		(struct cmsghdr *)NULL : 				\
		(struct cmsghdr *)((unsigned char *)(cmsg) + 		\
		CMSG_ALIGN((cmsg)->cmsg_len))) )

#define CMSG_DATA(cmsg) \
	( (unsigned char *)(cmsg) + CMSG_ALIGN(sizeof(struct cmsghdr)) )

#define CMSG_SPACE(l)	(CMSG_ALIGN(sizeof(struct cmsghdr)) + CMSG_ALIGN(l))
#define CMSG_LEN(l)	(CMSG_ALIGN(sizeof(struct cmsghdr)) + (l))

/* "Socket"-level control message types: */
#define SCM_RIGHTS	0x01
#define SCM_CREDENTIALS	0x02
#define SCM_SECURITY	0x04


/*
 * Types of socket shutdown(2).
 */
#define	SHUT_RD		0		/* Disallow further receives. */
#define	SHUT_WR		1		/* Disallow further sends. */
#define	SHUT_RDWR	2		/* Disallow further sends/receives. */

#include <sys/cdefs.h>

__BEGIN_DECLS
int	__cmsg_alignbytes(void);
__END_DECLS

__BEGIN_DECLS
int	accept(int, struct sockaddr * __restrict, socklen_t * __restrict);
int	bind(int, const struct sockaddr *, socklen_t);
int	connect(int, const struct sockaddr *, socklen_t);
int	getpeername(int, struct sockaddr * __restrict, socklen_t * __restrict);
int	getsockname(int, struct sockaddr * __restrict, socklen_t * __restrict);
int	getsockopt(int, int, int, void *__restrict, socklen_t * __restrict);
int	listen(int, int);
ssize_t	recv(int, void *, size_t, int);
ssize_t	recvfrom(int, void *__restrict, size_t, int,
	    struct sockaddr * __restrict, socklen_t * __restrict);
ssize_t	recvmsg(int, struct msghdr *, int);
ssize_t	send(int, const void *, size_t, int);
ssize_t	sendto(int, const void *,
	    size_t, int, const struct sockaddr *, socklen_t);
ssize_t	sendmsg(int, const struct msghdr *, int);
int	setsockopt(int, int, int, const void *, socklen_t);
int	shutdown(int, int);
int	sockatmark(int);
int	socket(int, int, int);
int	socketpair(int, int, int, int *);
__END_DECLS


#endif /* !_SYS_SOCKET_H_ */
