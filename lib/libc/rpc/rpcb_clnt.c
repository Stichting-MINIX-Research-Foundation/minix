/*	$NetBSD: rpcb_clnt.c,v 1.31 2015/03/26 11:31:57 justin Exp $	*/

/*
 * Copyright (c) 2010, Oracle America, Inc.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are
 * met:
 *
 *     * Redistributions of source code must retain the above copyright
 *       notice, this list of conditions and the following disclaimer.
 *     * Redistributions in binary form must reproduce the above
 *       copyright notice, this list of conditions and the following
 *       disclaimer in the documentation and/or other materials
 *       provided with the distribution.
 *     * Neither the name of the "Oracle America, Inc." nor the names of its
 *       contributors may be used to endorse or promote products derived
 *       from this software without specific prior written permission.
 *
 *   THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 *   "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 *   LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS
 *   FOR A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE
 *   COPYRIGHT HOLDER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT,
 *   INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 *   DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE
 *   GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 *   INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY,
 *   WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING
 *   NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 *   OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */
/*
 * Copyright (c) 1986-1991 by Sun Microsystems Inc. 
 */

/* #ident	"@(#)rpcb_clnt.c	1.27	94/04/24 SMI" */

#include <sys/cdefs.h>
#if defined(LIBC_SCCS) && !defined(lint)
#if 0
static char sccsid[] = "@(#)rpcb_clnt.c 1.30 89/06/21 Copyr 1988 Sun Micro";
#else
__RCSID("$NetBSD: rpcb_clnt.c,v 1.31 2015/03/26 11:31:57 justin Exp $");
#endif
#endif

/*
 * rpcb_clnt.c
 * interface to rpcbind rpc service.
 *
 * Copyright (C) 1988, Sun Microsystems, Inc.
 */

#include "namespace.h"
#include "reentrant.h"
#include <sys/types.h>
#include <sys/socket.h>
#include <sys/un.h>
#include <sys/utsname.h>
#include <rpc/rpc.h>
#include <rpc/rpcb_prot.h>
#include <rpc/nettype.h>
#include <netconfig.h>
#ifdef PORTMAP
#include <netinet/in.h>		/* FOR IPPROTO_TCP/UDP definitions */
#include <rpc/pmap_prot.h>
#endif
#include <assert.h>
#include <errno.h>
#include <netdb.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <syslog.h>
#include <unistd.h>

#include "svc_fdset.h"
#include "rpc_internal.h"

#ifdef __weak_alias
__weak_alias(rpcb_set,_rpcb_set)
__weak_alias(rpcb_unset,_rpcb_unset)
__weak_alias(rpcb_getmaps,_rpcb_getmaps)
__weak_alias(rpcb_taddr2uaddr,_rpcb_taddr2uaddr)
__weak_alias(rpcb_uaddr2taddr,_rpcb_uaddr2taddr)
#endif

static struct timeval tottimeout = { 60, 0 };
static const struct timeval rmttimeout = { 3, 0 };

static const char nullstring[] = "\000";

#define	CACHESIZE 6

struct address_cache {
	char *ac_host;
	char *ac_netid;
	char *ac_uaddr;
	struct netbuf *ac_taddr;
	struct address_cache *ac_next;
};

static struct address_cache *front;
static int cachesize;

#define	CLCR_GET_RPCB_TIMEOUT	1
#define	CLCR_SET_RPCB_TIMEOUT	2


extern int __rpc_lowvers;

static struct address_cache *check_cache(const char *, const char *);
static void delete_cache(struct netbuf *);
static void add_cache(const char *, const char *, struct netbuf *, char *);
static CLIENT *getclnthandle(const char *, const struct netconfig *, char **);
static CLIENT *local_rpcb(void);
static struct netbuf *got_entry(rpcb_entry_list_ptr, const struct netconfig *);

/*
 * This routine adjusts the timeout used for calls to the remote rpcbind.
 * Also, this routine can be used to set the use of portmapper version 2
 * only when doing rpc_broadcasts
 * These are private routines that may not be provided in future releases.
 */
bool_t
__rpc_control(int request, void *info)
{

	_DIAGASSERT(info != NULL);

	switch (request) {
	case CLCR_GET_RPCB_TIMEOUT:
		*(struct timeval *)info = tottimeout;
		break;
	case CLCR_SET_RPCB_TIMEOUT:
		tottimeout = *(struct timeval *)info;
		break;
	case CLCR_SET_LOWVERS:
		__rpc_lowvers = *(int *)info;
		break;
	case CLCR_GET_LOWVERS:
		*(int *)info = __rpc_lowvers;
		break;
	default:
		return (FALSE);
	}
	return (TRUE);
}

/*
 *	It might seem that a reader/writer lock would be more reasonable here.
 *	However because getclnthandle(), the only user of the cache functions,
 *	may do a delete_cache() operation if a check_cache() fails to return an
 *	address useful to clnt_tli_create(), we may as well use a mutex.
 */
/*
 * As it turns out, if the cache lock is *not* a reader/writer lock, we will
 * block all clnt_create's if we are trying to connect to a host that's down,
 * since the lock will be held all during that time.
 */
#ifdef _REENTRANT
extern rwlock_t	rpcbaddr_cache_lock;
#endif

/*
 * The routines check_cache(), add_cache(), delete_cache() manage the
 * cache of rpcbind addresses for (host, netid).
 */

static struct address_cache *
check_cache(const char *host, const char *netid)
{
	struct address_cache *cptr;

	_DIAGASSERT(host != NULL);
	_DIAGASSERT(netid != NULL);

	/* READ LOCK HELD ON ENTRY: rpcbaddr_cache_lock */

	for (cptr = front; cptr != NULL; cptr = cptr->ac_next) {
		if (!strcmp(cptr->ac_host, host) &&
		    !strcmp(cptr->ac_netid, netid)) {
#ifdef ND_DEBUG
			fprintf(stderr, "Found cache entry for %s: %s\n",
				host, netid);
#endif
			return (cptr);
		}
	}
	return NULL;
}

static void
delete_cache(struct netbuf *addr)
{
	struct address_cache *cptr, *prevptr = NULL;

	_DIAGASSERT(addr != NULL);

	/* WRITE LOCK HELD ON ENTRY: rpcbaddr_cache_lock */
	for (cptr = front; cptr != NULL; cptr = cptr->ac_next) {
		if (!memcmp(cptr->ac_taddr->buf, addr->buf, addr->len)) {
			free(cptr->ac_host);
			free(cptr->ac_netid);
			free(cptr->ac_taddr->buf);
			free(cptr->ac_taddr);
			if (cptr->ac_uaddr)
				free(cptr->ac_uaddr);
			if (prevptr)
				prevptr->ac_next = cptr->ac_next;
			else
				front = cptr->ac_next;
			free(cptr);
			cachesize--;
			break;
		}
		prevptr = cptr;
	}
}

static void
add_cache(const char *host, const char *netid, struct netbuf *taddr,
	char *uaddr)
{
	struct address_cache  *ad_cache, *cptr, *prevptr;

	_DIAGASSERT(host != NULL);
	_DIAGASSERT(netid != NULL);
	/* uaddr may be NULL */
	/* taddr may be NULL ??? */

	ad_cache = malloc(sizeof(*ad_cache));
	if (!ad_cache) {
		return;
	}
	ad_cache->ac_host = strdup(host);
	ad_cache->ac_netid = strdup(netid);
	ad_cache->ac_uaddr = uaddr ? strdup(uaddr) : NULL;
	ad_cache->ac_taddr = malloc(sizeof(*ad_cache->ac_taddr));
	if (!ad_cache->ac_host || !ad_cache->ac_netid || !ad_cache->ac_taddr ||
		(uaddr && !ad_cache->ac_uaddr)) {
		goto out;
	}
	ad_cache->ac_taddr->len = ad_cache->ac_taddr->maxlen = taddr->len;
	ad_cache->ac_taddr->buf = malloc(taddr->len);
	if (ad_cache->ac_taddr->buf == NULL) {
out:
		if (ad_cache->ac_host)
			free(ad_cache->ac_host);
		if (ad_cache->ac_netid)
			free(ad_cache->ac_netid);
		if (ad_cache->ac_uaddr)
			free(ad_cache->ac_uaddr);
		if (ad_cache->ac_taddr)
			free(ad_cache->ac_taddr);
		free(ad_cache);
		return;
	}
	memcpy(ad_cache->ac_taddr->buf, taddr->buf, taddr->len);
#ifdef ND_DEBUG
	fprintf(stderr, "Added to cache: %s : %s\n", host, netid);
#endif

/* VARIABLES PROTECTED BY rpcbaddr_cache_lock:  cptr */

	rwlock_wrlock(&rpcbaddr_cache_lock);
	if (cachesize < CACHESIZE) {
		ad_cache->ac_next = front;
		front = ad_cache;
		cachesize++;
	} else {
		/* Free the last entry */
		cptr = front;
		prevptr = NULL;
		while (cptr->ac_next) {
			prevptr = cptr;
			cptr = cptr->ac_next;
		}

#ifdef ND_DEBUG
		fprintf(stderr, "Deleted from cache: %s : %s\n",
			cptr->ac_host, cptr->ac_netid);
#endif
		free(cptr->ac_host);
		free(cptr->ac_netid);
		free(cptr->ac_taddr->buf);
		free(cptr->ac_taddr);
		if (cptr->ac_uaddr)
			free(cptr->ac_uaddr);

		if (prevptr) {
			prevptr->ac_next = NULL;
			ad_cache->ac_next = front;
			front = ad_cache;
		} else {
			front = ad_cache;
			ad_cache->ac_next = NULL;
		}
		free(cptr);
	}
	rwlock_unlock(&rpcbaddr_cache_lock);
}

/*
 * This routine will return a client handle that is connected to the
 * rpcbind. Returns NULL on error and free's everything.
 */
static CLIENT *
getclnthandle(const char *host, const struct netconfig *nconf, char **targaddr)
{
	CLIENT *client;
	struct netbuf *addr, taddr;
	struct netbuf addr_to_delete;
	struct __rpc_sockinfo si;
	struct addrinfo hints, *res, *tres;
	struct address_cache *ad_cache;
	char *tmpaddr;

	_DIAGASSERT(host != NULL);
	_DIAGASSERT(nconf != NULL);
	/* targaddr may be NULL */

/* VARIABLES PROTECTED BY rpcbaddr_cache_lock:  ad_cache */

	/* Get the address of the rpcbind.  Check cache first */
	client = NULL;
	addr_to_delete.len = 0;
	addr_to_delete.buf = NULL;
	rwlock_rdlock(&rpcbaddr_cache_lock);
	ad_cache = check_cache(host, nconf->nc_netid);
	if (ad_cache != NULL) {
		addr = ad_cache->ac_taddr;
		client = clnt_tli_create(RPC_ANYFD, nconf, addr,
		    (rpcprog_t)RPCBPROG, (rpcvers_t)RPCBVERS4, 0, 0);
		if (client != NULL) {
			if (targaddr)
				*targaddr = ad_cache->ac_uaddr;
			rwlock_unlock(&rpcbaddr_cache_lock);
			return (client);
		}
		addr_to_delete.len = addr->len;
		addr_to_delete.buf = malloc(addr->len);
		if (addr_to_delete.buf == NULL) {
			addr_to_delete.len = 0;
		} else {
			memcpy(addr_to_delete.buf, addr->buf, addr->len);
		}
	}
	rwlock_unlock(&rpcbaddr_cache_lock);
	if (addr_to_delete.len != 0) {
		/*
		 * Assume this may be due to cache data being
		 *  outdated
		 */
		rwlock_wrlock(&rpcbaddr_cache_lock);
		delete_cache(&addr_to_delete);
		rwlock_unlock(&rpcbaddr_cache_lock);
		free(addr_to_delete.buf);
	}
	if (!__rpc_nconf2sockinfo(nconf, &si)) {
		rpc_createerr.cf_stat = RPC_UNKNOWNPROTO;
		return NULL;
	}

	memset(&hints, 0, sizeof hints);
	hints.ai_family = si.si_af;
	hints.ai_socktype = si.si_socktype;
	hints.ai_protocol = si.si_proto;

#ifdef CLNT_DEBUG
	printf("trying netid %s family %d proto %d socktype %d\n",
	    nconf->nc_netid, si.si_af, si.si_proto, si.si_socktype);
#endif

	if (getaddrinfo(host, "sunrpc", &hints, &res) != 0) {
		rpc_createerr.cf_stat = RPC_UNKNOWNHOST;
		return NULL;
	}

	for (tres = res; tres != NULL; tres = tres->ai_next) {
		taddr.buf = tres->ai_addr;
		taddr.len = taddr.maxlen = tres->ai_addrlen;

#ifdef ND_DEBUG
		{
			char *ua;

			ua = taddr2uaddr(nconf, &taddr);
			fprintf(stderr, "Got it [%s]\n", ua);
			free(ua);
		}
#endif

#ifdef ND_DEBUG
		{
			int i;

			fprintf(stderr, "\tnetbuf len = %d, maxlen = %d\n",
				taddr.len, taddr.maxlen);
			fprintf(stderr, "\tAddress is ");
			for (i = 0; i < taddr.len; i++)
				fprintf(stderr, "%u.", ((char *)(taddr.buf))[i]);
			fprintf(stderr, "\n");
		}
#endif
		client = clnt_tli_create(RPC_ANYFD, nconf, &taddr,
		    (rpcprog_t)RPCBPROG, (rpcvers_t)RPCBVERS4, 0, 0);
#ifdef ND_DEBUG
		if (! client) {
			clnt_pcreateerror("rpcbind clnt interface");
		}
#endif

		if (client) {
			tmpaddr = targaddr ? taddr2uaddr(nconf, &taddr) : NULL;
			add_cache(host, nconf->nc_netid, &taddr, tmpaddr);
			if (targaddr)
				*targaddr = tmpaddr;
			break;
		}
	}
	freeaddrinfo(res);
	return (client);
}

/* XXX */
#define IN4_LOCALHOST_STRING	"127.0.0.1"
#define IN6_LOCALHOST_STRING	"::1"

/*
 * This routine will return a client handle that is connected to the local
 * rpcbind. Returns NULL on error and free's everything.
 */
static CLIENT *
local_rpcb(void)
{
	CLIENT *client;
	static struct netconfig *loopnconf;
	static const char *hostname;
#ifdef _REENTRANT
	extern mutex_t loopnconf_lock;
#endif
	int sock;
	size_t tsize;
	struct netbuf nbuf;
	struct sockaddr_un sun;

	/*
	 * Try connecting to the local rpcbind through a local socket
	 * first. If this doesn't work, try all transports defined in
	 * the netconfig file.
	 */
	memset(&sun, 0, sizeof sun);
	sock = socket(AF_LOCAL, SOCK_STREAM, 0);
	if (sock < 0)
		goto try_nconf;
	sun.sun_family = AF_LOCAL;
	strcpy(sun.sun_path, _PATH_RPCBINDSOCK);
	tsize = SUN_LEN(&sun);
	_DIAGASSERT(__type_fit(uint8_t, tsize));
	nbuf.len = sun.sun_len = (uint8_t)tsize;
	nbuf.maxlen = sizeof (struct sockaddr_un);
	nbuf.buf = &sun;

	tsize = __rpc_get_t_size(AF_LOCAL, 0, 0);
	_DIAGASSERT(__type_fit(u_int, tsize));
	client = clnt_vc_create(sock, &nbuf, (rpcprog_t)RPCBPROG,
	    (rpcvers_t)RPCBVERS, (u_int)tsize, (u_int)tsize);

	if (client != NULL) {
		/* XXX - mark the socket to be closed in destructor */
		(void) CLNT_CONTROL(client, CLSET_FD_CLOSE, NULL);
		return client;
	}

	/* XXX - nobody needs this socket anymore, free the descriptor */
	close(sock);

try_nconf:

/* VARIABLES PROTECTED BY loopnconf_lock: loopnconf */
	mutex_lock(&loopnconf_lock);
	if (loopnconf == NULL) {
		struct netconfig *nconf, *tmpnconf = NULL;
		void *nc_handle;
		int fd;

		nc_handle = setnetconfig();
		if (nc_handle == NULL) {
			/* fails to open netconfig file */
			syslog (LOG_ERR, "rpc: failed to open " NETCONFIG);
			rpc_createerr.cf_stat = RPC_UNKNOWNPROTO;
			mutex_unlock(&loopnconf_lock);
			return (NULL);
		}
		while ((nconf = getnetconfig(nc_handle)) != NULL) {
#ifdef INET6
			if ((strcmp(nconf->nc_protofmly, NC_INET6) == 0 ||
#else
			if ((
#endif
			     strcmp(nconf->nc_protofmly, NC_INET) == 0) &&
			    (nconf->nc_semantics == NC_TPI_COTS ||
			     nconf->nc_semantics == NC_TPI_COTS_ORD)) {
				fd = __rpc_nconf2fd(nconf);
				/*
				 * Can't create a socket, assume that
				 * this family isn't configured in the kernel.
				 */
				if (fd < 0)
					continue;
				close(fd);
				tmpnconf = nconf;
				if (!strcmp(nconf->nc_protofmly, NC_INET))
					hostname = IN4_LOCALHOST_STRING;
				else
					hostname = IN6_LOCALHOST_STRING;
			}
		}
		if (tmpnconf == NULL) {
			rpc_createerr.cf_stat = RPC_UNKNOWNPROTO;
			mutex_unlock(&loopnconf_lock);
			return (NULL);
		}
		loopnconf = getnetconfigent(tmpnconf->nc_netid);
		/* loopnconf is never freed */
		endnetconfig(nc_handle);
	}
	mutex_unlock(&loopnconf_lock);
	client = getclnthandle(hostname, loopnconf, NULL);
	return (client);
}

/*
 * Set a mapping between program, version and address.
 * Calls the rpcbind service to do the mapping.
 */
bool_t
rpcb_set(rpcprog_t program, rpcvers_t version,
	const struct netconfig *nconf,	/* Network structure of transport */
	const struct netbuf *address)	/* Services netconfig address */
{
	CLIENT *client;
	bool_t rslt = FALSE;
	RPCB parms;
	char uidbuf[32];

	/* parameter checking */
	if (nconf == NULL) {
		rpc_createerr.cf_stat = RPC_UNKNOWNPROTO;
		return (FALSE);
	}
	if (address == NULL) {
		rpc_createerr.cf_stat = RPC_UNKNOWNADDR;
		return (FALSE);
	}
	client = local_rpcb();
	if (! client) {
		return (FALSE);
	}

	/* convert to universal */
	parms.r_addr = taddr2uaddr(__UNCONST(nconf), __UNCONST(address));
	if (!parms.r_addr) {
		CLNT_DESTROY(client);
		rpc_createerr.cf_stat = RPC_N2AXLATEFAILURE;
		return (FALSE); /* no universal address */
	}
	parms.r_prog = program;
	parms.r_vers = version;
	parms.r_netid = nconf->nc_netid;
	/*
	 * Though uid is not being used directly, we still send it for
	 * completeness.  For non-unix platforms, perhaps some other
	 * string or an empty string can be sent.
	 */
	(void) snprintf(uidbuf, sizeof uidbuf, "%d", geteuid());
	parms.r_owner = uidbuf;

	if (CLNT_CALL(client, (rpcproc_t)RPCBPROC_SET, (xdrproc_t) xdr_rpcb,
	    (char *)(void *)&parms, (xdrproc_t) xdr_bool,
	    (char *)(void *)&rslt, tottimeout) != RPC_SUCCESS) {
		rpc_createerr.cf_stat = RPC_PMAPFAILURE;
		clnt_geterr(client, &rpc_createerr.cf_error);
	}

	CLNT_DESTROY(client);
	free(parms.r_addr);
	return (rslt);
}

/*
 * Remove the mapping between program, version and netbuf address.
 * Calls the rpcbind service to do the un-mapping.
 * If netbuf is NULL, unset for all the transports, otherwise unset
 * only for the given transport.
 */
bool_t
rpcb_unset(rpcprog_t program, rpcvers_t version, const struct netconfig *nconf)
{
	CLIENT *client;
	bool_t rslt = FALSE;
	RPCB parms;
	char uidbuf[32];

	client = local_rpcb();
	if (! client) {
		return (FALSE);
	}

	parms.r_prog = program;
	parms.r_vers = version;
	if (nconf)
		parms.r_netid = nconf->nc_netid;
	else {
		parms.r_netid = __UNCONST(&nullstring[0]); /* unsets  all */
	}
	parms.r_addr = __UNCONST(&nullstring[0]);
	(void) snprintf(uidbuf, sizeof uidbuf, "%d", geteuid());
	parms.r_owner = uidbuf;

	if (CLNT_CALL(client, (rpcproc_t)RPCBPROC_UNSET, (xdrproc_t) xdr_rpcb,
	    (char *)(void *)&parms, (xdrproc_t) xdr_bool,
	    (char *)(void *)&rslt, tottimeout) != RPC_SUCCESS) {
		rpc_createerr.cf_stat = RPC_PMAPFAILURE;
		clnt_geterr(client, &rpc_createerr.cf_error);
	}

	CLNT_DESTROY(client);
	return (rslt);
}

/*
 * From the merged list, find the appropriate entry
 */
static struct netbuf *
got_entry(rpcb_entry_list_ptr relp, const struct netconfig *nconf)
{
	struct netbuf *na = NULL;
	rpcb_entry_list_ptr sp;
	rpcb_entry *rmap;

	_DIAGASSERT(nconf != NULL);

	for (sp = relp; sp != NULL; sp = sp->rpcb_entry_next) {
		rmap = &sp->rpcb_entry_map;
		if ((strcmp(nconf->nc_proto, rmap->r_nc_proto) == 0) &&
		    (strcmp(nconf->nc_protofmly, rmap->r_nc_protofmly) == 0) &&
		    (nconf->nc_semantics == rmap->r_nc_semantics) &&
		    (rmap->r_maddr != NULL) && (rmap->r_maddr[0] != 0)) {
			na = uaddr2taddr(nconf, rmap->r_maddr);
#ifdef ND_DEBUG
			fprintf(stderr, "\tRemote address is [%s].\n",
				rmap->r_maddr);
			if (!na)
				fprintf(stderr,
				    "\tCouldn't resolve remote address!\n");
#endif
			break;
		}
	}
	return (na);
}

/*
 * An internal function which optimizes rpcb_getaddr function.  It also
 * returns the client handle that it uses to contact the remote rpcbind.
 *
 * The algorithm used: If the transports is TCP or UDP, it first tries
 * version 2 (portmap), 4 and then 3 (svr4).  This order should be
 * changed in the next OS release to 4, 2 and 3.  We are assuming that by
 * that time, version 4 would be available on many machines on the network.
 * With this algorithm, we get performance as well as a plan for
 * obsoleting version 2.
 *
 * For all other transports, the algorithm remains as 4 and then 3.
 *
 * XXX: Due to some problems with t_connect(), we do not reuse the same client
 * handle for COTS cases and hence in these cases we do not return the
 * client handle.  This code will change if t_connect() ever
 * starts working properly.  Also look under clnt_vc.c.
 */
struct netbuf *
__rpcb_findaddr(rpcprog_t program, rpcvers_t version,
	const struct netconfig *nconf, const char *host, CLIENT **clpp)
{
	CLIENT *client = NULL;
	RPCB parms;
	enum clnt_stat clnt_st;
	char *ua = NULL;
	rpcvers_t vers;
	struct netbuf *address = NULL;
	rpcvers_t start_vers = RPCBVERS4;
	struct netbuf servaddr;

	/* nconf is handled below */
	_DIAGASSERT(host != NULL);
	/* clpp may be NULL */

	/* parameter checking */
	if (nconf == NULL) {
		rpc_createerr.cf_stat = RPC_UNKNOWNPROTO;
		return (NULL);
	}

	parms.r_addr = NULL;

#ifdef PORTMAP
	/* Try version 2 for TCP or UDP */
	if (strcmp(nconf->nc_protofmly, NC_INET) == 0) {
		u_short port = 0;
		struct netbuf remote;
		rpcvers_t pmapvers = 2;
		struct pmap pmapparms;

		/*
		 * Try UDP only - there are some portmappers out
		 * there that use UDP only.
		 */
		if (strcmp(nconf->nc_proto, NC_TCP) == 0) {
			struct netconfig *newnconf;

			if ((newnconf = getnetconfigent("udp")) == NULL) {
				rpc_createerr.cf_stat = RPC_UNKNOWNPROTO;
				return (NULL);
			}
			client = getclnthandle(host, newnconf, &parms.r_addr);
			freenetconfigent(newnconf);
		} else {
			client = getclnthandle(host, nconf, &parms.r_addr);
		}
		if (client == NULL) {
			return (NULL);
		}

		/* Set the version */
		CLNT_CONTROL(client, CLSET_VERS, (char *)(void *)&pmapvers);
		pmapparms.pm_prog = program;
		pmapparms.pm_vers = version;
		pmapparms.pm_prot = strcmp(nconf->nc_proto, NC_TCP) ?
					IPPROTO_UDP : IPPROTO_TCP;
		pmapparms.pm_port = 0;	/* not needed */
		clnt_st = CLNT_CALL(client, (rpcproc_t)PMAPPROC_GETPORT,
		    (xdrproc_t) xdr_pmap, (caddr_t)(void *)&pmapparms,
		    (xdrproc_t) xdr_u_short, (caddr_t)(void *)&port,
		    tottimeout);
		if (clnt_st != RPC_SUCCESS) {
			if ((clnt_st == RPC_PROGVERSMISMATCH) ||
				(clnt_st == RPC_PROGUNAVAIL))
				goto try_rpcbind; /* Try different versions */
			rpc_createerr.cf_stat = RPC_PMAPFAILURE;
			clnt_geterr(client, &rpc_createerr.cf_error);
			goto error;
		} else if (port == 0) {
			address = NULL;
			rpc_createerr.cf_stat = RPC_PROGNOTREGISTERED;
			goto error;
		}
		port = htons(port);
		CLNT_CONTROL(client, CLGET_SVC_ADDR, (char *)(void *)&remote);
		if (((address = malloc(sizeof(struct netbuf))) == NULL) ||
		    ((address->buf = malloc(remote.len)) == NULL)) {
			rpc_createerr.cf_stat = RPC_SYSTEMERROR;
			clnt_geterr(client, &rpc_createerr.cf_error);
			if (address) {
				free(address);
				address = NULL;
			}
			goto error;
		}
		memcpy(address->buf, remote.buf, remote.len);
		memcpy(&((char *)address->buf)[sizeof (short)],
				(char *)(void *)&port, sizeof (short));
		address->len = address->maxlen = remote.len;
		goto done;
	}
#endif

try_rpcbind:
	/*
	 * Now we try version 4 and then 3.
	 * We also send the remote system the address we used to
	 * contact it in case it can help to connect back with us
	 */
	parms.r_prog = program;
	parms.r_vers = version;
	parms.r_owner = __UNCONST(&nullstring[0]);	/* not needed; */
							/* just for xdring */
	parms.r_netid = nconf->nc_netid; /* not really needed */

	/*
	 * If a COTS transport is being used, try getting address via CLTS
	 * transport.  This works only with version 4.
	 * NOTE: This is being done for all transports EXCEPT LOOPBACK
	 * because with loopback the cost to go to a COTS is same as
	 * the cost to go through CLTS, plus you get the advantage of
	 * finding out immediately if the local rpcbind process is dead.
	 */
#if 1
	if ((nconf->nc_semantics == NC_TPI_COTS_ORD ||
			nconf->nc_semantics == NC_TPI_COTS) &&
	    (strcmp(nconf->nc_protofmly, NC_LOOPBACK) != 0))
#else
	if (client != NULL) {
		CLNT_DESTROY(client);
		client = NULL;
	}
	if (nconf->nc_semantics == NC_TPI_CLTS)
#endif
	{
		void *handle;
		struct netconfig *nconf_clts;
		rpcb_entry_list_ptr relp = NULL;

		if (client == NULL) {
			/* This did not go through the above PORTMAP/TCP code */
#if 1
			if ((handle = __rpc_setconf("datagram_v")) != NULL)
#else
			if ((handle = __rpc_setconf("circuit_v")) != NULL)
#endif
			{
				while ((nconf_clts = __rpc_getconf(handle))
					!= NULL) {
					if (strcmp(nconf_clts->nc_protofmly,
						nconf->nc_protofmly) != 0) {
						continue;
					}
					client = getclnthandle(host, nconf_clts,
							&parms.r_addr);
					break;
				}
				__rpc_endconf(handle);
			}
			if (client == NULL)
				goto regular_rpcbind;	/* Go the regular way */
		} else {
			/* This is a UDP PORTMAP handle.  Change to version 4 */
			vers = RPCBVERS4;
			CLNT_CONTROL(client, CLSET_VERS, (char *)(void *)&vers);
		}
		/*
		 * We also send the remote system the address we used to
		 * contact it in case it can help it connect back with us
		 */
		if (parms.r_addr == NULL) {
			/* for XDRing */
			parms.r_addr = __UNCONST(&nullstring[0]); 
		}
		clnt_st = CLNT_CALL(client, (rpcproc_t)RPCBPROC_GETADDRLIST,
		    (xdrproc_t) xdr_rpcb, (char *)(void *)&parms,
		    (xdrproc_t) xdr_rpcb_entry_list_ptr,
		    (char *)(void *)&relp, tottimeout);
		if (clnt_st == RPC_SUCCESS) {
			if ((address = got_entry(relp, nconf)) != NULL) {
				xdr_free((xdrproc_t) xdr_rpcb_entry_list_ptr,
				    (char *)(void *)&relp);
				CLNT_CONTROL(client, CLGET_SVC_ADDR,
					(char *)(void *)&servaddr);
				__rpc_fixup_addr(address, &servaddr);
				goto done;
			}
			/* Entry not found for this transport */
			xdr_free((xdrproc_t) xdr_rpcb_entry_list_ptr,
			    (char *)(void *)&relp);
			/*
			 * XXX: should have perhaps returned with error but
			 * since the remote machine might not always be able
			 * to send the address on all transports, we try the
			 * regular way with regular_rpcbind
			 */
			goto regular_rpcbind;
		} else if ((clnt_st == RPC_PROGVERSMISMATCH) ||
			(clnt_st == RPC_PROGUNAVAIL)) {
			start_vers = RPCBVERS;	/* Try version 3 now */
			goto regular_rpcbind; /* Try different versions */
		} else {
			rpc_createerr.cf_stat = RPC_PMAPFAILURE;
			clnt_geterr(client, &rpc_createerr.cf_error);
			goto error;
		}
	}

regular_rpcbind:

	/* Now the same transport is to be used to get the address */
#if 1
	if (client && ((nconf->nc_semantics == NC_TPI_COTS_ORD) ||
			(nconf->nc_semantics == NC_TPI_COTS)))
#else
	if (client && nconf->nc_semantics == NC_TPI_CLTS)
#endif
	{
		/* A CLTS type of client - destroy it */
		CLNT_DESTROY(client);
		client = NULL;
	}

	if (client == NULL) {
		client = getclnthandle(host, nconf, &parms.r_addr);
		if (client == NULL) {
			goto error;
		}
	}
	if (parms.r_addr == NULL)
		parms.r_addr = __UNCONST(&nullstring[0]);

	/* First try from start_vers and then version 3 (RPCBVERS) */
	for (vers = start_vers;  vers >= RPCBVERS; vers--) {
		/* Set the version */
		CLNT_CONTROL(client, CLSET_VERS, (char *)(void *)&vers);
		clnt_st = CLNT_CALL(client, (rpcproc_t)RPCBPROC_GETADDR,
		    (xdrproc_t) xdr_rpcb, (char *)(void *)&parms,
		    (xdrproc_t) xdr_wrapstring, (char *)(void *) &ua,
		    tottimeout);
		if (clnt_st == RPC_SUCCESS) {
			if ((ua == NULL) || (ua[0] == 0)) {
				/* address unknown */
				rpc_createerr.cf_stat = RPC_PROGNOTREGISTERED;
				goto error;
			}
			address = uaddr2taddr(nconf, ua);
#ifdef ND_DEBUG
			fprintf(stderr, "\tRemote address is [%s]\n", ua);
			if (!address)
				fprintf(stderr,
					"\tCouldn't resolve remote address!\n");
#endif
			xdr_free((xdrproc_t)xdr_wrapstring,
			    (char *)(void *)&ua);

			if (! address) {
				/* We don't know about your universal address */
				rpc_createerr.cf_stat = RPC_N2AXLATEFAILURE;
				goto error;
			}
			CLNT_CONTROL(client, CLGET_SVC_ADDR,
			    (char *)(void *)&servaddr);
			__rpc_fixup_addr(address, &servaddr);
			goto done;
		} else if (clnt_st == RPC_PROGVERSMISMATCH) {
			struct rpc_err rpcerr;

			clnt_geterr(client, &rpcerr);
			if (rpcerr.re_vers.low > RPCBVERS4)
				goto error;  /* a new version, can't handle */
		} else if (clnt_st != RPC_PROGUNAVAIL) {
			/* Cant handle this error */
			rpc_createerr.cf_stat = clnt_st;
			clnt_geterr(client, &rpc_createerr.cf_error);
			goto error;
		}
	}

error:
	if (client) {
		CLNT_DESTROY(client);
		client = NULL;
	}
done:
	if (nconf->nc_semantics != NC_TPI_CLTS) {
		/* This client is the connectionless one */
		if (client) {
			CLNT_DESTROY(client);
			client = NULL;
		}
	}
	if (clpp) {
		*clpp = client;
	} else if (client) {
		CLNT_DESTROY(client);
	}
	return (address);
}


/*
 * Find the mapped address for program, version.
 * Calls the rpcbind service remotely to do the lookup.
 * Uses the transport specified in nconf.
 * Returns FALSE (0) if no map exists, else returns 1.
 *
 * Assuming that the address is all properly allocated
 */
bool_t
rpcb_getaddr(const rpcprog_t program, const rpcvers_t version,
	const struct netconfig *nconf, struct netbuf *address,
	const char *host)
{
	struct netbuf *na;

	_DIAGASSERT(address != NULL);

	if ((na = __rpcb_findaddr(program, version, nconf,
				host, NULL)) == NULL)
		return (FALSE);

	if (na->len > address->maxlen) {
		/* Too long address */
		free(na->buf);
		free(na);
		rpc_createerr.cf_stat = RPC_FAILED;
		return (FALSE);
	}
	memcpy(address->buf, na->buf, (size_t)na->len);
	address->len = na->len;
	free(na->buf);
	free(na);
	return (TRUE);
}

/*
 * Get a copy of the current maps.
 * Calls the rpcbind service remotely to get the maps.
 *
 * It returns only a list of the services
 * It returns NULL on failure.
 */
rpcblist *
rpcb_getmaps(const struct netconfig *nconf, const char *host)
{
	rpcblist_ptr head = NULL;
	CLIENT *client;
	enum clnt_stat clnt_st;
	rpcvers_t vers = 0;

	client = getclnthandle(host, nconf, NULL);
	if (client == NULL) {
		return (head);
	}
	clnt_st = CLNT_CALL(client, (rpcproc_t)RPCBPROC_DUMP,
	    (xdrproc_t) xdr_void, NULL, (xdrproc_t) xdr_rpcblist_ptr,
	    (char *)(void *)&head, tottimeout);
	if (clnt_st == RPC_SUCCESS)
		goto done;

	if ((clnt_st != RPC_PROGVERSMISMATCH) &&
	    (clnt_st != RPC_PROGUNAVAIL)) {
		rpc_createerr.cf_stat = RPC_RPCBFAILURE;
		clnt_geterr(client, &rpc_createerr.cf_error);
		goto done;
	}

	/* fall back to earlier version */
	CLNT_CONTROL(client, CLGET_VERS, (char *)(void *)&vers);
	if (vers == RPCBVERS4) {
		vers = RPCBVERS;
		CLNT_CONTROL(client, CLSET_VERS, (char *)(void *)&vers);
		if (CLNT_CALL(client, (rpcproc_t)RPCBPROC_DUMP,
		    (xdrproc_t) xdr_void, NULL, (xdrproc_t) xdr_rpcblist_ptr,
		    (char *)(void *)&head, tottimeout) == RPC_SUCCESS)
			goto done;
	}
	rpc_createerr.cf_stat = RPC_RPCBFAILURE;
	clnt_geterr(client, &rpc_createerr.cf_error);

done:
	CLNT_DESTROY(client);
	return (head);
}

/*
 * rpcbinder remote-call-service interface.
 * This routine is used to call the rpcbind remote call service
 * which will look up a service program in the address maps, and then
 * remotely call that routine with the given parameters. This allows
 * programs to do a lookup and call in one step.
*/
enum clnt_stat
rpcb_rmtcall(
	const struct netconfig *nconf,	/* Netconfig structure */
	const char *host,		/* Remote host name */
	rpcprog_t prog,
	rpcvers_t vers,
	rpcproc_t proc,			/* Remote proc identifiers */
	xdrproc_t xdrargs,
	const char *argsp,		/* Argument */
	xdrproc_t xdrres,		/* XDR routines */
	caddr_t resp,			/* Result */
	struct timeval tout,		/* Timeout value for this call */
	const struct netbuf *addr_ptr)	/* Preallocated netbuf address */
{
	CLIENT *client;
	enum clnt_stat stat;
	struct r_rpcb_rmtcallargs a;
	struct r_rpcb_rmtcallres r;
	rpcvers_t rpcb_vers;

	stat = RPC_FAILED;	/* XXXGCC -Wuninitialized [dreamcast] */

	client = getclnthandle(host, nconf, NULL);
	if (client == NULL) {
		return (RPC_FAILED);
	}
	CLNT_CONTROL(client, CLSET_RETRY_TIMEOUT, __UNCONST(&rmttimeout));
	a.prog = prog;
	a.vers = vers;
	a.proc = proc;
	a.args.args_val = argsp;
	a.xdr_args = xdrargs;
	r.addr = NULL;
	r.results.results_val = resp;
	r.xdr_res = xdrres;

	for (rpcb_vers = RPCBVERS4; rpcb_vers >= RPCBVERS; rpcb_vers--) {
		CLNT_CONTROL(client, CLSET_VERS, (char *)(void *)&rpcb_vers);
		stat = CLNT_CALL(client, (rpcproc_t)RPCBPROC_CALLIT,
		    (xdrproc_t) xdr_rpcb_rmtcallargs, (char *)(void *)&a,
		    (xdrproc_t) xdr_rpcb_rmtcallres, (char *)(void *)&r, tout);
		if ((stat == RPC_SUCCESS) && (addr_ptr != NULL)) {
			struct netbuf *na;
			na = uaddr2taddr(__UNCONST(nconf), r.addr);
			if (!na) {
				stat = RPC_N2AXLATEFAILURE;
				((struct netbuf *)__UNCONST(addr_ptr))->len = 0;
				goto error;
			}
			if (na->len > addr_ptr->maxlen) {
				/* Too long address */
				stat = RPC_FAILED; /* XXX A better error no */
				free(na->buf);
				free(na);
				((struct netbuf *)__UNCONST(addr_ptr))->len = 0;
				goto error;
			}
			memcpy(addr_ptr->buf, na->buf, (size_t)na->len);
			((struct netbuf *)__UNCONST(addr_ptr))->len = na->len;
			free(na->buf);
			free(na);
			break;
		} else if ((stat != RPC_PROGVERSMISMATCH) &&
			    (stat != RPC_PROGUNAVAIL)) {
			goto error;
		}
	}
error:
	CLNT_DESTROY(client);
	if (r.addr)
		xdr_free((xdrproc_t) xdr_wrapstring, (char *)(void *)&r.addr);
	return (stat);
}

/*
 * Gets the time on the remote host.
 * Returns 1 if succeeds else 0.
 */
bool_t
rpcb_gettime(const char *host, time_t *timep)
{
	CLIENT *client = NULL;
	void *handle;
	struct netconfig *nconf;
	rpcvers_t vers;
	enum clnt_stat st;


	if ((host == NULL) || (host[0] == 0)) {
		time(timep);
		return (TRUE);
	}

	if ((handle = __rpc_setconf("netpath")) == NULL) {
		rpc_createerr.cf_stat = RPC_UNKNOWNPROTO;
		return (FALSE);
	}
	rpc_createerr.cf_stat = RPC_SUCCESS;
	while (client == NULL) {
		if ((nconf = __rpc_getconf(handle)) == NULL) {
			if (rpc_createerr.cf_stat == RPC_SUCCESS)
				rpc_createerr.cf_stat = RPC_UNKNOWNPROTO;
			break;
		}
		client = getclnthandle(host, nconf, NULL);
		if (client)
			break;
	}
	__rpc_endconf(handle);
	if (client == NULL) {
		return (FALSE);
	}

	st = CLNT_CALL(client, (rpcproc_t)RPCBPROC_GETTIME,
		(xdrproc_t) xdr_void, NULL,
		(xdrproc_t) xdr_int, (char *)(void *)timep, tottimeout);

	if ((st == RPC_PROGVERSMISMATCH) || (st == RPC_PROGUNAVAIL)) {
		CLNT_CONTROL(client, CLGET_VERS, (char *)(void *)&vers);
		if (vers == RPCBVERS4) {
			/* fall back to earlier version */
			vers = RPCBVERS;
			CLNT_CONTROL(client, CLSET_VERS, (char *)(void *)&vers);
			st = CLNT_CALL(client, (rpcproc_t)RPCBPROC_GETTIME,
				(xdrproc_t) xdr_void, NULL,
				(xdrproc_t) xdr_int, (char *)(void *)timep,
				tottimeout);
		}
	}
	CLNT_DESTROY(client);
	return (st == RPC_SUCCESS? TRUE: FALSE);
}

/*
 * Converts taddr to universal address.  This routine should never
 * really be called because local n2a libraries are always provided.
 */
char *
rpcb_taddr2uaddr(struct netconfig *nconf, struct netbuf *taddr)
{
	CLIENT *client;
	char *uaddr = NULL;

	/* parameter checking */
	if (nconf == NULL) {
		rpc_createerr.cf_stat = RPC_UNKNOWNPROTO;
		return (NULL);
	}
	if (taddr == NULL) {
		rpc_createerr.cf_stat = RPC_UNKNOWNADDR;
		return (NULL);
	}
	client = local_rpcb();
	if (! client) {
		return (NULL);
	}

	if (CLNT_CALL(client, (rpcproc_t)RPCBPROC_TADDR2UADDR,
	    (xdrproc_t) xdr_netbuf, (char *)(void *)taddr,
	    (xdrproc_t) xdr_wrapstring, (char *)(void *)&uaddr, tottimeout) 
	    != RPC_SUCCESS) {
		rpc_createerr.cf_stat = RPC_PMAPFAILURE;
		clnt_geterr(client, &rpc_createerr.cf_error);
	}
	CLNT_DESTROY(client);
	return (uaddr);
}

/*
 * Converts universal address to netbuf.  This routine should never
 * really be called because local n2a libraries are always provided.
 */
struct netbuf *
rpcb_uaddr2taddr(struct netconfig *nconf, char *uaddr)
{
	CLIENT *client;
	struct netbuf *taddr;


	/* parameter checking */
	if (nconf == NULL) {
		rpc_createerr.cf_stat = RPC_UNKNOWNPROTO;
		return (NULL);
	}
	if (uaddr == NULL) {
		rpc_createerr.cf_stat = RPC_UNKNOWNADDR;
		return (NULL);
	}
	client = local_rpcb();
	if (! client) {
		return (NULL);
	}

	taddr = (struct netbuf *)calloc(1, sizeof (struct netbuf));
	if (taddr == NULL) {
		CLNT_DESTROY(client);
		return (NULL);
	}
	if (CLNT_CALL(client, (rpcproc_t)RPCBPROC_UADDR2TADDR,
	    (xdrproc_t) xdr_wrapstring, (char *)(void *)&uaddr,
	    (xdrproc_t) xdr_netbuf, (char *)(void *)taddr,
	    tottimeout) != RPC_SUCCESS) {
		rpc_createerr.cf_stat = RPC_PMAPFAILURE;
		clnt_geterr(client, &rpc_createerr.cf_error);
		free(taddr);
		taddr = NULL;
	}
	CLNT_DESTROY(client);
	return (taddr);
}
