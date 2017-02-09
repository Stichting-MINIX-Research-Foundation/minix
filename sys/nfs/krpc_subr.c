/*	$NetBSD: krpc_subr.c,v 1.41 2015/05/21 02:04:22 rtr Exp $	*/

/*
 * Copyright (c) 1995 Gordon Ross, Adam Glass
 * Copyright (c) 1992 Regents of the University of California.
 * All rights reserved.
 *
 * This software was developed by the Computer Systems Engineering group
 * at Lawrence Berkeley Laboratory under DARPA contract BG 91-66 and
 * contributed to Berkeley.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 3. All advertising materials mentioning features or use of this software
 *    must display the following acknowledgement:
 *	This product includes software developed by the University of
 *	California, Lawrence Berkeley Laboratory and its contributors.
 * 4. Neither the name of the University nor the names of its contributors
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
 * partially based on:
 *      libnetboot/rpc.c
 *               @(#) Header: rpc.c,v 1.12 93/09/28 08:31:56 leres Exp  (LBL)
 */

#include <sys/cdefs.h>
__KERNEL_RCSID(0, "$NetBSD: krpc_subr.c,v 1.41 2015/05/21 02:04:22 rtr Exp $");

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/ioctl.h>
#include <sys/proc.h>
#include <sys/mount.h>
#include <sys/mbuf.h>
#include <sys/reboot.h>
#include <sys/socket.h>
#include <sys/socketvar.h>

#include <netinet/in.h>

#include <nfs/rpcv2.h>
#include <nfs/krpc.h>
#include <nfs/xdr_subs.h>
#include <nfs/nfsproto.h> /* XXX NFSX_V3FHMAX for next */
#include <nfs/nfs.h>
#include <nfs/nfsmount.h>
#include <nfs/nfsdiskless.h> /* XXX decl nfs_boot_sendrecv */

/*
 * Kernel support for Sun RPC
 *
 * Used currently for bootstrapping in nfs diskless configurations.
 */

/*
 * Generic RPC headers
 */

struct auth_info {
	u_int32_t 	authtype;	/* auth type */
	u_int32_t	authlen;	/* auth length */
};

struct auth_unix {
	int32_t   ua_time;
	int32_t   ua_hostname;	/* null */
	int32_t   ua_uid;
	int32_t   ua_gid;
	int32_t   ua_gidlist;	/* null */
};

struct rpc_call {
	u_int32_t	rp_xid;		/* request transaction id */
	int32_t 	rp_direction;	/* call direction (0) */
	u_int32_t	rp_rpcvers;	/* rpc version (2) */
	u_int32_t	rp_prog;	/* program */
	u_int32_t	rp_vers;	/* version */
	u_int32_t	rp_proc;	/* procedure */
	struct	auth_info rpc_auth;
	struct	auth_unix rpc_unix;
	struct	auth_info rpc_verf;
};

struct rpc_reply {
	u_int32_t rp_xid;		/* request transaction id */
	int32_t  rp_direction;		/* call direction (1) */
	int32_t  rp_astatus;		/* accept status (0: accepted) */
	union {
		/* rejected */
		struct {
			u_int32_t rej_stat;
			u_int32_t rej_val1;
			u_int32_t rej_val2;
		} rpu_rej;
		/* accepted */
		struct {
			struct auth_info rok_auth;
			u_int32_t	rok_status;
		} rpu_rok;
	} rp_u;
};
#define rp_rstat  rp_u.rpu_rej.rej_stat
#define rp_auth   rp_u.rpu_rok.rok_auth
#define rp_status rp_u.rpu_rok.rok_status

#define MIN_REPLY_HDR 16	/* xid, dir, astat, errno */

static int krpccheck(struct mbuf**, void*);

/*
 * Call portmap to lookup a port number for a particular rpc program
 * Returns non-zero error on failure.
 */
int
krpc_portmap(struct sockaddr_in *sin, u_int prog, u_int vers, u_int proto, u_int16_t *portp, struct lwp *l)
	/* sin:		 server address */
	/* prog, vers, proto:	 host order */
	/* portp:	 network order */
{
	struct sdata {
		u_int32_t prog;		/* call program */
		u_int32_t vers;		/* call version */
		u_int32_t proto;	/* call protocol */
		u_int32_t port;		/* call port (unused) */
	} *sdata;
	struct rdata {
		u_int16_t pad;
		u_int16_t port;
	} *rdata;
	struct mbuf *m;
	int error;

	/* The portmapper port is fixed. */
	if (prog == PMAPPROG) {
		*portp = htons(PMAPPORT);
		return 0;
	}

	m = m_get(M_WAIT, MT_DATA);
	sdata = mtod(m, struct sdata *);
	m->m_len = sizeof(*sdata);

	/* Do the RPC to get it. */
	sdata->prog = txdr_unsigned(prog);
	sdata->vers = txdr_unsigned(vers);
	sdata->proto = txdr_unsigned(proto);
	sdata->port = 0;

	sin->sin_port = htons(PMAPPORT);
	error = krpc_call(sin, PMAPPROG, PMAPVERS,
					  PMAPPROC_GETPORT, &m, NULL, l);
	if (error)
		return error;

	if (m->m_len < sizeof(*rdata)) {
		m = m_pullup(m, sizeof(*rdata));
		if (m == NULL)
			return ENOBUFS;
	}
	rdata = mtod(m, struct rdata *);
	*portp = rdata->port;

	m_freem(m);
	return 0;
}

static int krpccheck(struct mbuf **mp, void *context)
{
	struct rpc_reply *reply;
	struct mbuf *m = *mp;

	/* Does the reply contain at least a header? */
	if (m->m_pkthdr.len < MIN_REPLY_HDR)
		return(-1);
	if (m->m_len < sizeof(struct rpc_reply)) {
		m = *mp = m_pullup(m, sizeof(struct rpc_reply));
		if (m == NULL)
			return(-1);
	}
	reply = mtod(m, struct rpc_reply *);

	/* Is it the right reply? */
	if (reply->rp_direction != txdr_unsigned(RPC_REPLY))
		return(-1);

	if (reply->rp_xid != txdr_unsigned(*(u_int32_t*)context))
		return(-1);

	return(0);
}

/*
 * Do a remote procedure call (RPC) and wait for its reply.
 * If from_p is non-null, then we are doing broadcast, and
 * the address from whence the response came is saved there.
 */
int
krpc_call(struct sockaddr_in *sa, u_int prog, u_int vers, u_int func, struct mbuf **data, struct mbuf **from_p, struct lwp *l)
	/* data:	 input/output */
	/* from_p:	 output */
{
	struct socket *so;
	struct sockaddr_in sin;
	struct mbuf *m, *mhead, *from;
	struct rpc_call *call;
	struct rpc_reply *reply;
	int error, len;
	static u_int32_t xid = ~0xFF;
	u_int16_t tport;

	/*
	 * Validate address family.
	 * Sorry, this is INET specific...
	 */
	if (sa->sin_family != AF_INET)
		return (EAFNOSUPPORT);

	/* Free at end if not null. */
	mhead = NULL;
	from = NULL;

	/*
	 * Create socket and set its receive timeout.
	 */
	if ((error = socreate(AF_INET, &so, SOCK_DGRAM, 0, l, NULL)))
		return error;

	if ((error = nfs_boot_setrecvtimo(so)))
		goto out;

	/*
	 * Enable broadcast if necessary.
	 */
	if (from_p) {
		if ((error = nfs_boot_enbroadcast(so)))
			goto out;
	}

	/*
	 * Bind the local endpoint to a reserved port,
	 * because some NFS servers refuse requests from
	 * non-reserved (non-privileged) ports.
	 */
	tport = IPPORT_RESERVED;
	do {
		tport--;
		error = nfs_boot_sobind_ipport(so, tport, l);
	} while (error == EADDRINUSE &&
			 tport > IPPORT_RESERVED / 2);
	if (error) {
		printf("bind failed\n");
		goto out;
	}

	/*
	 * Setup socket address for the server.
	 */
	sin = *sa;

	/*
	 * Prepend RPC message header.
	 */
	mhead = m_gethdr(M_WAIT, MT_DATA);
	mhead->m_next = *data;
	call = mtod(mhead, struct rpc_call *);
	mhead->m_len = sizeof(*call);
	memset((void *)call, 0, sizeof(*call));
	/* rpc_call part */
	xid++;
	call->rp_xid = txdr_unsigned(xid);
	/* call->rp_direction = 0; */
	call->rp_rpcvers = txdr_unsigned(2);
	call->rp_prog = txdr_unsigned(prog);
	call->rp_vers = txdr_unsigned(vers);
	call->rp_proc = txdr_unsigned(func);
	/* rpc_auth part (auth_unix as root) */
	call->rpc_auth.authtype = txdr_unsigned(RPCAUTH_UNIX);
	call->rpc_auth.authlen  = txdr_unsigned(sizeof(struct auth_unix));
	/* rpc_verf part (auth_null) */
	call->rpc_verf.authtype = 0;
	call->rpc_verf.authlen  = 0;

	/*
	 * Setup packet header
	 */
	len = 0;
	m = mhead;
	while (m) {
		len += m->m_len;
		m = m->m_next;
	}
	mhead->m_pkthdr.len = len;
	mhead->m_pkthdr.rcvif = NULL;

	error = nfs_boot_sendrecv(so, &sin, NULL, mhead, krpccheck, &m, &from,
	    &xid, l);
	if (error)
		goto out;

	/* m_pullup() was done in krpccheck() */
	reply = mtod(m, struct rpc_reply *);

	/* Was RPC accepted? (authorization OK) */
	if (reply->rp_astatus != 0) {
		/* Note: This is NOT an error code! */
		error = fxdr_unsigned(u_int32_t, reply->rp_rstat);
		switch (error) {
		    case RPC_MISMATCH:
			/* .re_status = RPC_VERSMISMATCH; */
			error = ERPCMISMATCH;
			break;
		    case RPC_AUTHERR:
			/* .re_status = RPC_AUTHERROR; */
			error = EAUTH;
			break;
		    default:
			/* unexpected */
			error = EBADRPC;
			break;
		}
		goto out;
	}

	/* Did the call succeed? */
	if (reply->rp_status != 0) {
		/* Note: This is NOT an error code! */
		error = fxdr_unsigned(u_int32_t, reply->rp_status);
		switch (error) {
		    case RPC_PROGUNAVAIL:
			error = EPROGUNAVAIL;
			break;
		    case RPC_PROGMISMATCH:
			error = EPROGMISMATCH;
			break;
		    case RPC_PROCUNAVAIL:
			error = EPROCUNAVAIL;
			break;
		    case RPC_GARBAGE:
		    default:
			error = EBADRPC;
		}
		goto out;
	}

	/*
	 * OK, we have received a good reply!
	 * Get its length, then strip it off.
	 */
	len = sizeof(*reply);
	if (reply->rp_auth.authtype != 0) {
		len += fxdr_unsigned(u_int32_t, reply->rp_auth.authlen);
		len = (len + 3) & ~3; /* XXX? */
	}
	m_adj(m, len);

	/* result */
	*data = m;
	if (from_p && error == 0) {
		*from_p = from;
		from = NULL;
	}

 out:
	if (mhead) m_freem(mhead);
	if (from) m_freem(from);
	soclose(so);
	return error;
}

/*
 * eXternal Data Representation routines.
 * (but with non-standard args...)
 */

/*
 * String representation for RPC.
 */
struct xdr_string {
	u_int32_t len;		/* length without null or padding */
	char data[4];	/* data (longer, of course) */
    /* data is padded to a long-word boundary */
};

struct mbuf *
xdr_string_encode(char *str, int len)
{
	struct mbuf *m;
	struct xdr_string *xs;
	int dlen;	/* padded string length */
	int mlen;	/* message length */

	dlen = (len + 3) & ~3;
	mlen = dlen + 4;

	if (mlen > MCLBYTES)		/* If too big, we just can't do it. */
		return (NULL);

	m = m_get(M_WAIT, MT_DATA);
	if (mlen > MLEN) {
		m_clget(m, M_WAIT);
		if ((m->m_flags & M_EXT) == 0) {
			(void) m_free(m);	/* There can be only one. */
			return (NULL);
		}
	}
	xs = mtod(m, struct xdr_string *);
	m->m_len = mlen;
	xs->len = txdr_unsigned(len);
	memcpy(xs->data, str, len);
	return (m);
}

struct mbuf *
xdr_string_decode(struct mbuf *m, char *str, int *len_p)
	/* len_p:		 bufsize - 1 */
{
	struct xdr_string *xs;
	int mlen;	/* message length */
	int slen;	/* string length */

	if (m->m_len < 4) {
		m = m_pullup(m, 4);
		if (m == NULL)
			return (NULL);
	}
	xs = mtod(m, struct xdr_string *);
	slen = fxdr_unsigned(u_int32_t, xs->len);
	mlen = 4 + ((slen + 3) & ~3);

	if (slen > *len_p)
		slen = *len_p;
	m_copydata(m, 4, slen, str);
	m_adj(m, mlen);

	str[slen] = '\0';
	*len_p = slen;

	return (m);
}


/*
 * Inet address in RPC messages
 * (Note, really four ints, NOT chars.  Blech.)
 */
struct xdr_inaddr {
	u_int32_t atype;
	u_int32_t addr[4];
};

struct mbuf *
xdr_inaddr_encode(struct in_addr *ia)
	/* ia:		 already in network order */
{
	struct mbuf *m;
	struct xdr_inaddr *xi;
	u_int8_t *cp;
	u_int32_t *ip;

	m = m_get(M_WAIT, MT_DATA);
	xi = mtod(m, struct xdr_inaddr *);
	m->m_len = sizeof(*xi);
	xi->atype = txdr_unsigned(1);
	ip = xi->addr;
	cp = (u_int8_t *)&ia->s_addr;
	*ip++ = txdr_unsigned(*cp++);
	*ip++ = txdr_unsigned(*cp++);
	*ip++ = txdr_unsigned(*cp++);
	*ip++ = txdr_unsigned(*cp++);

	return (m);
}

struct mbuf *
xdr_inaddr_decode(struct mbuf *m, struct in_addr *ia)
	/* ia:		 already in network order */
{
	struct xdr_inaddr *xi;
	u_int8_t *cp;
	u_int32_t *ip;

	if (m->m_len < sizeof(*xi)) {
		m = m_pullup(m, sizeof(*xi));
		if (m == NULL)
			return (NULL);
	}
	xi = mtod(m, struct xdr_inaddr *);
	if (xi->atype != txdr_unsigned(1)) {
		ia->s_addr = INADDR_ANY;
		goto out;
	}
	ip = xi->addr;
	cp = (u_int8_t *)&ia->s_addr;
	*cp++ = fxdr_unsigned(u_int8_t, *ip++);
	*cp++ = fxdr_unsigned(u_int8_t, *ip++);
	*cp++ = fxdr_unsigned(u_int8_t, *ip++);
	*cp++ = fxdr_unsigned(u_int8_t, *ip++);

out:
	m_adj(m, sizeof(*xi));
	return (m);
}
