/*	sys/ioc_net.h - NetBSD-friendly version of Minix net/ioctl.h 
 */
/*	net/ioctl.h - Network ioctl() command codes.	Author: Kees J. Bot
 *								23 Nov 2002
 *
 */

#ifndef _SYS_IOC_NET_H_
#define _SYS_IOC_NET_H_

#include <minix/ioctl.h>
#include <sys/un.h>

#include <sys/ansi.h>
#ifndef socklen_t
typedef __socklen_t	socklen_t;
#define socklen_t	__socklen_t
#endif

#define MSG_CONTROL_MAX (1024 - sizeof(socklen_t))
struct msg_control
{
	char		msg_control[MSG_CONTROL_MAX];
	socklen_t	msg_controllen;
};


/* Network ioctls. */
#define NWIOSETHOPT	_IOW('n', 16, struct nwio_ethopt)
#define NWIOGETHOPT	_IOR('n', 17, struct nwio_ethopt)
#define NWIOGETHSTAT	_IOR('n', 18, struct nwio_ethstat)

#define NWIOARPGIP	_IORW('n',20, struct nwio_arp)
#define NWIOARPGNEXT	_IORW('n',21, struct nwio_arp)
#define NWIOARPSIP	_IOW ('n',22, struct nwio_arp)
#define NWIOARPDIP	_IOW ('n',23, struct nwio_arp)

#define NWIOSIPCONF2	_IOW('n', 32, struct nwio_ipconf2)
#define NWIOSIPCONF	_IOW('n', 32, struct nwio_ipconf)
#define NWIOGIPCONF2	_IOR('n', 33, struct nwio_ipconf2)
#define NWIOGIPCONF	_IOR('n', 33, struct nwio_ipconf)
#define NWIOSIPOPT	_IOW('n', 34, struct nwio_ipopt)
#define NWIOGIPOPT	_IOR('n', 35, struct nwio_ipopt)

#define NWIOGIPOROUTE	_IORW('n', 40, struct nwio_route)
#define NWIOSIPOROUTE	_IOW ('n', 41, struct nwio_route)
#define NWIODIPOROUTE	_IOW ('n', 42, struct nwio_route)
#define NWIOGIPIROUTE	_IORW('n', 43, struct nwio_route)
#define NWIOSIPIROUTE	_IOW ('n', 44, struct nwio_route)
#define NWIODIPIROUTE	_IOW ('n', 45, struct nwio_route)

#define NWIOSTCPCONF	_IOW('n', 48, struct nwio_tcpconf)
#define NWIOGTCPCONF	_IOR('n', 49, struct nwio_tcpconf)
#define NWIOTCPCONN	_IOW('n', 50, struct nwio_tcpcl)
#define NWIOTCPLISTEN	_IOW('n', 51, struct nwio_tcpcl)
#define NWIOTCPATTACH	_IOW('n', 52, struct nwio_tcpatt)
#define NWIOTCPSHUTDOWN	_IO ('n', 53)
#define NWIOSTCPOPT	_IOW('n', 54, struct nwio_tcpopt)
#define NWIOGTCPOPT	_IOR('n', 55, struct nwio_tcpopt)
#define NWIOTCPPUSH	_IO ('n', 56)
#define NWIOTCPLISTENQ	_IOW('n', 57, int)
#define NWIOGTCPCOOKIE	_IOR('n', 58, struct tcp_cookie)
#define NWIOTCPACCEPTTO	_IOW('n', 59, struct tcp_cookie)
#define NWIOTCPGERROR	_IOR('n', 60, int)

#define NWIOSUDPOPT	_IOW('n', 64, struct nwio_udpopt)
#define NWIOGUDPOPT	_IOR('n', 65, struct nwio_udpopt)
#define NWIOUDPPEEK	_IOR('n', 66, struct udp_io_hdr)

#define NWIOGUDSFADDR   _IOR ('n', 67, struct sockaddr_un) /* recvfrom() */
#define NWIOSUDSTADDR	_IOW ('n', 68, struct sockaddr_un) /* sendto() */
#define NWIOSUDSADDR	_IOW ('n', 69, struct sockaddr_un) /* bind() */
#define NWIOGUDSADDR	_IOR ('n', 70, struct sockaddr_un) /* getsockname() */
#define NWIOGUDSPADDR	_IOR ('n', 71, struct sockaddr_un) /* getpeername() */
#define NWIOSUDSTYPE	_IOW ('n', 72, int)		   /* socket() */
#define NWIOSUDSBLOG	_IOW ('n', 73, int)		   /* listen() */
#define NWIOSUDSCONN	_IOW ('n', 74, struct sockaddr_un) /* connect() */
#define NWIOSUDSSHUT    _IOW ('n', 75, int)		  /* shutdown() */
#define NWIOSUDSPAIR	_IOW ('n', 76, dev_t)		  /* socketpair() */
#define NWIOSUDSPAIROLD	_IOW ('n', 76, short)		  /* socketpair() */
#define NWIOSUDSACCEPT	_IOW ('n', 77, struct sockaddr_un) /* accept() */
#define NWIOSUDSCTRL	_IOW ('n', 78, struct msg_control) /* sendmsg() */
#define NWIOGUDSCTRL	_IORW('n', 79, struct msg_control) /* recvmsg() */

#define NWIOSPSIPOPT	_IOW('n', 80, struct nwio_psipopt)
#define NWIOGPSIPOPT	_IOR('n', 81, struct nwio_psipopt)

/* setsockopt/setsockopt for unix domain sockets */
#define NWIOGUDSSOTYPE	 _IOR('n', 90, int)		  /* SO_TYPE */
#define NWIOGUDSPEERCRED _IOR('n', 91, struct ucred)	  /* SO_PEERCRED */
#define NWIOGUDSPEERCREDOLD _IOR('n', 91, struct ucred_old)	  /* SO_PEERCRED */
#define NWIOGUDSSNDBUF	 _IOR('n', 92, size_t)            /* SO_SNDBUF */
#define NWIOSUDSSNDBUF	 _IOW('n', 93, size_t)            /* SO_SNDBUF */
#define NWIOGUDSRCVBUF	 _IOR('n', 94, size_t)            /* SO_RCVBUF */
#define NWIOSUDSRCVBUF	 _IOW('n', 95, size_t)            /* SO_RCVBUF */

#endif /* _NET__IOCTL_H */

/*
 * $PchId: ioctl.h,v 1.2 2003/07/25 14:34:03 philip Exp $
 */
