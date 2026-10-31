/*	$NetBSD: bootparam.c,v 1.21 2019/04/02 22:25:10 christos Exp $	*/

/*
 * Copyright (c) 1995 Gordon W. Ross
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
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR ``AS IS'' AND ANY EXPRESS OR
 * IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES
 * OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED.
 * IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR ANY DIRECT, INDIRECT,
 * INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT
 * NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF
 * THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

/*
 * RPC/bootparams
 */

#include <sys/param.h>
#include <sys/socket.h>

#include <net/if.h>

#include <netinet/in.h>
#include <netinet/in_systm.h>

#ifdef _STANDALONE
#include <lib/libkern/libkern.h>
#else
#include <string.h>
#endif

#include "rpcv2.h"

#include "stand.h"
#include "net.h"
#include "rpc.h"
#include "bootparam.h"

#ifdef DEBUG_RPC
#define RPC_PRINTF(a)	printf a
#else
#define RPC_PRINTF(a)
#endif

struct in_addr	bp_server_addr;	/* net order */
n_short		bp_server_port;	/* net order */

uint32_t	hostnamelen;
char		domainname[FNAME_SIZE]; /* our DNS domain */
uint32_t	domainnamelen;

/*
 * RPC definitions for bootparamd
 */
#define	BOOTPARAM_PROG		100026
#define	BOOTPARAM_VERS		1
#define BOOTPARAM_WHOAMI	1
#define BOOTPARAM_GETFILE	2

/*
 * Inet address in RPC messages
 * (Note, really four ints, NOT chars.  Blech.)
 */
struct xdr_inaddr {
	uint32_t  atype;
	int32_t	addr[4];
};

int xdr_inaddr_encode(char **, struct in_addr);
int xdr_inaddr_decode(char **, struct in_addr *);

int xdr_string_encode(char **, char *, uint32_t);
int xdr_string_decode(char **, char *, uint32_t *);


/*
 * RPC: bootparam/whoami
 * Given client IP address, get:
 *	client name	(hostname)
 *	domain name (domainname)
 *	gateway address
 *
 * The hostname and domainname are set here for convenience.
 *
 * Note - bpsin is initialized to the broadcast address,
 * and will be replaced with the bootparam server address
 * after this call is complete.  Have to use PMAP_PROC_CALL
 * to make sure we get responses only from a servers that
 * know about us (don't want to broadcast a getport call).
 */
int
bp_whoami(int sockfd)
{
	/* RPC structures for PMAPPROC_CALLIT */
	struct args {
		uint32_t prog;
		uint32_t vers;
		uint32_t proc;
		uint32_t arglen;
		struct xdr_inaddr xina;
	} *args;
	struct repl {
		uint16_t _pad;
		uint16_t port;
		uint32_t encap_len;
		/* encapsulated data here */
		n_long  capsule[64];
	} *repl;
	struct {
		n_long	h[RPC_HEADER_WORDS];
		struct args d;
	} sdata;
	struct {
		n_long	h[RPC_HEADER_WORDS];
		struct repl d;
	} rdata;
	char *send_tail, *recv_head;
	struct iodesc *d;
	uint32_t x;
	ssize_t len;

	RPC_PRINTF(("%s: myip=%s\n", __func__, inet_ntoa(myip)));

	if (!(d = socktodesc(sockfd))) {
		RPC_PRINTF(("%s: bad socket. %d\n", __func__, sockfd));
		return -1;
	}
	args = &sdata.d;
	repl = &rdata.d;

	/*
	 * Build request args for PMAPPROC_CALLIT.
	 */
	args->prog = htonl((uint32_t)BOOTPARAM_PROG);
	args->vers = htonl((uint32_t)BOOTPARAM_VERS);
	args->proc = htonl((uint32_t)BOOTPARAM_WHOAMI);
	args->arglen = htonl((uint32_t)sizeof(struct xdr_inaddr));
	send_tail = (char *)&args->xina;

	/*
	 * append encapsulated data (client IP address)
	 */
	if (xdr_inaddr_encode(&send_tail, myip))
		return -1;

	/* RPC: portmap/callit */
	--rpc_port;
	d->myport = (uint16_t)htons((uint16_t)rpc_port);
	d->destip.s_addr = INADDR_BROADCAST;	/* XXX: subnet bcast? */
	/* rpc_call will set d->destport */

	len = rpc_call(d, PMAPPROG, PMAPVERS, PMAPPROC_CALLIT,
				  args, (size_t)(send_tail - (char *)args),
				  repl, sizeof(*repl));
	if (len < 8) {
		printf("%s: 'whoami' call failed\n", __func__);
		return -1;
	}

	/* Save bootparam server address (from IP header). */
	rpc_fromaddr(repl, &bp_server_addr, &bp_server_port);

	/*
	 * Note that bp_server_port is now 111 due to the
	 * indirect call (using PMAPPROC_CALLIT), so get the
	 * actual port number from the reply data.
	 */
	bp_server_port = repl->port;

	RPC_PRINTF(("%s: server at %s:%d\n", __func__,
	    inet_ntoa(bp_server_addr), ntohs((uint16_t)bp_server_port)));

	/* We have just done a portmap call, so cache the portnum. */
	rpc_pmap_putcache(bp_server_addr,
			  BOOTPARAM_PROG,
			  BOOTPARAM_VERS,
			  (int)ntohs((uint16_t)bp_server_port));

	/*
	 * Parse the encapsulated results from bootparam/whoami
	 */
	x = ntohl((uint32_t)repl->encap_len);
	if (len < x) {
		printf("%s: short reply, %zd < %d\n", __func__, len, x);
		return -1;
	}
	recv_head = (char *)repl->capsule;

	/* client name */
	hostnamelen = MAXHOSTNAMELEN-1;
	if (xdr_string_decode(&recv_head, hostname, &hostnamelen)) {
		RPC_PRINTF(("%s: bad hostname\n", __func__));
		return -1;
	}

	/* domain name */
	domainnamelen = MAXHOSTNAMELEN-1;
	if (xdr_string_decode(&recv_head, domainname, &domainnamelen)) {
		RPC_PRINTF(("%s: bad domainname\n", __func__));
		return -1;
	}

	/* gateway address */
	if (xdr_inaddr_decode(&recv_head, &gateip)) {
		RPC_PRINTF(("%s: bad gateway\n", __func__));
		return -1;
	}

	/* success */
	return 0;
}


/*
 * RPC: bootparam/getfile
 * Given client name and file "key", get:
 *	server name
 *	server IP address
 *	server pathname
 */
int
bp_getfile(int sockfd, char *key, struct in_addr *serv_addr, char *pathname)
{
	struct {
		n_long	h[RPC_HEADER_WORDS];
		n_long  d[64];
	} sdata;
	struct {
		n_long	h[RPC_HEADER_WORDS];
		n_long  d[128];
	} rdata;
	char serv_name[FNAME_SIZE];
	char *send_tail, *recv_head;
	/* misc... */
	struct iodesc *d;
	uint32_t sn_len, path_len;
	ssize_t rlen;

	if (!(d = socktodesc(sockfd))) {
		RPC_PRINTF(("%s: bad socket. %d\n", __func__, sockfd));
		return -1;
	}

	send_tail = (char *)sdata.d;
	recv_head = (char *)rdata.d;

	/*
	 * Build request message.
	 */

	/* client name (hostname) */
	if (xdr_string_encode(&send_tail, hostname, hostnamelen)) {
		RPC_PRINTF(("%s: bad client\n", __func__));
		return -1;
	}

	/* key name (root or swap) */
	if (xdr_string_encode(&send_tail, key, (uint32_t)strlen(key))) {
		RPC_PRINTF(("%s: bad key\n", __func__));
		return -1;
	}

	/* RPC: bootparam/getfile */
	--rpc_port;
	d->myport = htons((uint16_t)rpc_port);
	d->destip = bp_server_addr;
	/* rpc_call will set d->destport */

	rlen = rpc_call(d,
		BOOTPARAM_PROG, BOOTPARAM_VERS, BOOTPARAM_GETFILE,
		sdata.d, (size_t)(send_tail - (char *)sdata.d),
		rdata.d, sizeof(rdata.d));
	if (rlen < 4) {
		RPC_PRINTF(("%s: short reply\n", __func__));
		errno = EBADRPC;
		return -1;
	}
	recv_head = (char *)rdata.d;

	/*
	 * Parse result message.
	 */

	/* server name */
	sn_len = FNAME_SIZE - 1;
	if (xdr_string_decode(&recv_head, serv_name, &sn_len)) {
		RPC_PRINTF(("%s: bad server name\n", __func__));
		return -1;
	}

	/* server IP address (mountd/NFS) */
	if (xdr_inaddr_decode(&recv_head, serv_addr)) {
		RPC_PRINTF(("%s: bad server addr\n", __func__));
		return -1;
	}

	/* server pathname */
	path_len = MAXPATHLEN - 1;
	if (xdr_string_decode(&recv_head, pathname, &path_len)) {
		RPC_PRINTF(("%s: bad server path\n", __func__));
		return -1;
	}

	/* success */
	return 0;
}


/*
 * eXternal Data Representation routines.
 * (but with non-standard args...)
 */


int
xdr_string_encode(char **pkt, char *str, uint32_t len)
{
	uint32_t *lenp;
	char *datap;
	uint32_t padlen = (len + 3) & ~3U;	/* padded length */

	/* The data will be int aligned. */
	lenp = (uint32_t *)*pkt;
	*pkt += sizeof(*lenp);
	*lenp = htonl(len);

	datap = *pkt;
	*pkt += padlen;
	(void)memcpy(datap, str, len);

	return 0;
}

/* len_p: bufsize - 1 */
int
xdr_string_decode(char **pkt, char *str, uint32_t *len_p)
{
	uint32_t *lenp;
	char *datap;
	uint32_t slen;	/* string length */
	uint32_t plen;	/* padded length */

	/* The data will be int aligned. */
	lenp = (uint32_t *)*pkt;
	*pkt += sizeof(*lenp);
	slen = ntohl(*lenp);
	plen = (slen + 3) & ~3U;

	if (slen > *len_p)
		slen = *len_p;
	datap = *pkt;
	*pkt += plen;
	(void)memcpy(str, datap, slen);

	str[slen] = '\0';
	*len_p = slen;

	return 0;
}


/* ia: network order */
int
xdr_inaddr_encode(char **pkt, struct in_addr ia)
{
	struct xdr_inaddr *xi;
	u_char *cp;
	uint32_t *ip;
	union {
		n_long l;	/* network order */
		u_char c[4];
	} uia;

	/* The data will be int aligned. */
	xi = (struct xdr_inaddr *)*pkt;
	*pkt += sizeof(*xi);
	xi->atype = htonl(1);
	uia.l = ia.s_addr;
	cp = uia.c;
	ip = (uint32_t *)xi->addr;
	/*
	 * Note: the htonl() calls below DO NOT
	 * imply that uia.l is in host order.
	 * In fact this needs it in net order.
	 */
	*ip++ = htonl((uint32_t)*cp++);
	*ip++ = htonl((uint32_t)*cp++);
	*ip++ = htonl((uint32_t)*cp++);
	*ip++ = htonl((uint32_t)*cp++);

	return 0;
}

/* ia: network order */
int
xdr_inaddr_decode(char **pkt, struct in_addr *ia)
{
	struct xdr_inaddr *xi;
	u_char *cp;
	uint32_t *ip;
	union {
		n_long l;	/* network order */
		u_char c[4];
	} uia;

	/* The data will be int aligned. */
	xi = (struct xdr_inaddr *)*pkt;
	*pkt += sizeof(*xi);
	if (xi->atype != htonl((uint32_t)1)) {
		RPC_PRINTF(("%s: bad addrtype=%d\n", __func__,
		    ntohl((uint32_t)nxi->atype)));
		return -1;
	}

	cp = uia.c;
	ip = (uint32_t *)xi->addr;
	/*
	 * Note: the ntohl() calls below DO NOT
	 * imply that uia.l is in host order.
	 * In fact this needs it in net order.
	 */
	*cp++ = (u_char)ntohl(*ip++);
	*cp++ = (u_char)ntohl(*ip++);
	*cp++ = (u_char)ntohl(*ip++);
	*cp++ = (u_char)ntohl(*ip++);
	ia->s_addr = uia.l;

	return 0;
}
