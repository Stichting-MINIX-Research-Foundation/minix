/*
net/netlib.h
*/

#ifndef _NET__NETLIB_H_
#define _NET__NETLIB_H_

#ifndef _ANSI
#include <ansi.h>
#endif

_PROTOTYPE (int iruserok, (unsigned long raddr, int superuser,
		const char *ruser, const char *luser) );
_PROTOTYPE (int rcmd, (char **ahost, int rport, const char *locuser, 
		const char *remuser, const char *cmd, int *fd2p) );

#define IPSTAT_DEVICE	"/dev/ipstat"
#define ETH_DEVICE	"/dev/eth"
#define IP_DEVICE	"/dev/ip"
#define TCP_DEVICE	"/dev/tcp"
#define UDP_DEVICE	"/dev/udp"
#define UDS_DEVICE	"/dev/uds"

#endif /* _NET__NETLIB_H_ */
