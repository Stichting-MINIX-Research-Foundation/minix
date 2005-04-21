/*	net/ioctl.h - Network ioctl() command codes.	Author: Kees J. Bot
 *								23 Nov 2002
 *
 */

#ifndef _NET__IOCTL_H
#define _NET__IOCTL_H

#include <minix/ioctl.h>

/* Network ioctls. */
#define NWIOSETHOPT	_IOW('n', 16, struct nwio_ethopt)
#define NWIOGETHOPT	_IOR('n', 17, struct nwio_ethopt)
#define NWIOGETHSTAT	_IOR('n', 18, struct nwio_ethstat)

#define NWIOSIPCONF	_IOW('n', 32, struct nwio_ipconf)
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

#define NWIOSUDPOPT	_IOW('n', 64, struct nwio_udpopt)
#define NWIOGUDPOPT	_IOR('n', 65, struct nwio_udpopt)

#define NWIOSPSIPOPT	_IOW('n', 80, struct nwio_psipopt)
#define NWIOGPSIPOPT	_IOR('n', 81, struct nwio_psipopt)

#endif /* _NET__IOCTL_H */
