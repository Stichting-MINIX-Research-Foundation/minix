/*	$NetBSD: sockin_user.c,v 1.1 2014/03/13 01:40:30 pooka Exp $	*/

/*
 * Copyright (c) 2008 Antti Kantee.  All Rights Reserved.
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
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR ``AS IS'' AND ANY EXPRESS
 * OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
 * WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
 * DISCLAIMED. IN NO EVENT SHALL THE AUTHOR OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR
 * SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 */

/* for struct msghdr content visibility */
#define _XOPEN_SOURCE 4
#define _XOPEN_SOURCE_EXTENDED 1

#ifndef _KERNEL
#include <sys/types.h>
#include <sys/socket.h>

#include <errno.h>
#include <poll.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>

#include <rump/rumpuser_component.h>
#include <rump/rumpdefs.h>

#include "sockin_user.h"

#define seterror(_v_) if ((_v_) == -1) rv = errno; else rv = 0;

#ifndef __arraycount
#define __arraycount(a) (sizeof(a) / sizeof(*a))
#endif

#ifndef __UNCONST
#define __UNCONST(a) ((void*)(const void*)a)
#endif

#include <netinet/in.h>
#include <netinet/tcp.h>
#include <netinet/udp.h>


static int translate_so_sockopt(int);
static int translate_ip_sockopt(int);
static int translate_tcp_sockopt(int);
static int translate_domain(int);

#define translate(_a_) case RUMP_##_a_: return _a_
static int
translate_so_sockopt(int lopt)
{

	switch (lopt) {
	translate(SO_DEBUG);
#ifndef SO_REUSEPORT
	case RUMP_SO_REUSEPORT: return SO_REUSEADDR;
#else
	translate(SO_REUSEPORT);
#endif
	translate(SO_TYPE);
	translate(SO_ERROR);
	translate(SO_DONTROUTE);
	translate(SO_BROADCAST);
	translate(SO_SNDBUF);
	translate(SO_RCVBUF);
	translate(SO_KEEPALIVE);
	translate(SO_OOBINLINE);
	translate(SO_LINGER);
	default: return -1;
	}
}

static int
translate_ip_sockopt(int lopt)
{

	switch (lopt) {
	translate(IP_TOS);
	translate(IP_TTL);
	translate(IP_HDRINCL);
	translate(IP_MULTICAST_TTL);
	translate(IP_MULTICAST_LOOP);
	translate(IP_MULTICAST_IF);
	translate(IP_ADD_MEMBERSHIP);
	translate(IP_DROP_MEMBERSHIP);
	default: return -1;
	}
}

static int
translate_tcp_sockopt(int lopt)
{

	switch (lopt) {
	translate(TCP_NODELAY);
	translate(TCP_MAXSEG);
	default: return -1;
	}
}

static int
translate_domain(int domain)
{

	switch (domain) {
	translate(AF_INET);
	translate(AF_INET6);
	default: return AF_UNSPEC;
	}
}

#undef translate

static void
translate_sockopt(int *levelp, int *namep)
{
	int level, name;

	level = *levelp;
	name = *namep;

	switch (level) {
	case RUMP_SOL_SOCKET:
		level = SOL_SOCKET;
		name = translate_so_sockopt(name);
		break;
	case RUMP_IPPROTO_IP:
#ifdef SOL_IP
		level = SOL_IP;
#else
		level = IPPROTO_IP;
#endif
		name = translate_ip_sockopt(name);
		break;
	case RUMP_IPPROTO_TCP:
#ifdef SOL_TCP
		level = SOL_TCP;
#else
		level = IPPROTO_TCP;
#endif
		name = translate_tcp_sockopt(name);
		break;
	case RUMP_IPPROTO_UDP:
#ifdef SOL_UDP
		level = SOL_UDP;
#else
		level = IPPROTO_UDP;
#endif
		name = -1;
		break;
	default:
		level = -1;
	}
	*levelp = level;
	*namep = name;
}

#ifndef __NetBSD__
static const struct {
	int bfl;
	int lfl;
} bsd_to_native_msg_flags_[] = {
	{RUMP_MSG_OOB,		MSG_OOB},
	{RUMP_MSG_PEEK,		MSG_PEEK},
	{RUMP_MSG_DONTROUTE,	MSG_DONTROUTE},
	{RUMP_MSG_EOR,		MSG_EOR},
	{RUMP_MSG_TRUNC,	MSG_TRUNC},
	{RUMP_MSG_CTRUNC,	MSG_CTRUNC},
	{RUMP_MSG_WAITALL,	MSG_WAITALL},
	{RUMP_MSG_DONTWAIT,	MSG_DONTWAIT},

	/* might be better to always set NOSIGNAL ... */
#ifdef MSG_NOSIGNAL
	{RUMP_MSG_NOSIGNAL,	MSG_NOSIGNAL},
#endif
};

static int native_to_bsd_msg_flags(int);

static int
native_to_bsd_msg_flags(int lflag)
{
	unsigned int i;
	int bfl, lfl;
	int bflag = 0;

	if (lflag == 0)
		return (0);

	for(i = 0; i < __arraycount(bsd_to_native_msg_flags_); i++) {
		bfl = bsd_to_native_msg_flags_[i].bfl;
		lfl = bsd_to_native_msg_flags_[i].lfl;

		if (lflag & lfl) {
			lflag ^= lfl;
			bflag |= bfl;
		}
	}
	if (lflag != 0)
		return (-1);

	return (bflag);
}

static int
bsd_to_native_msg_flags(int bflag)
{
	unsigned int i;
	int lflag = 0;

	if (bflag == 0)
		return (0);

	for(i = 0; i < __arraycount(bsd_to_native_msg_flags_); i++) {
		if (bflag & bsd_to_native_msg_flags_[i].bfl)
			lflag |= bsd_to_native_msg_flags_[i].lfl;
	}

	return (lflag);
}
#endif

struct rump_sockaddr {
	uint8_t	sa_len;	    /* total length */
	uint8_t	sa_family;	/* address family */
	char	sa_data[14];	/* actually longer; address value */
};

struct rump_msghdr {
	void		*msg_name;	/* optional address */
	uint32_t	msg_namelen;	/* size of address */
	struct iovec	*msg_iov;	/* scatter/gather array */
	int		msg_iovlen;	/* # elements in msg_iov */
	void		*msg_control;	/* ancillary data, see below */
	uint32_t	msg_controllen;	/* ancillary data buffer len */
	int		msg_flags;	/* flags on received message */
};

static struct sockaddr *translate_sockaddr(const struct sockaddr *,
		uint32_t);
static void translate_sockaddr_back(const struct sockaddr *,
		struct rump_sockaddr *, uint32_t len);
static struct msghdr *translate_msghdr(const struct rump_msghdr *, int *);
static void translate_msghdr_back(const struct msghdr *, struct rump_msghdr *);

#if defined(__NetBSD__)
static struct sockaddr *
translate_sockaddr(const struct sockaddr *addr, uint32_t len)
{

	return (struct sockaddr *)__UNCONST(addr);
}

static void
translate_sockaddr_back(const struct sockaddr *laddr,
		struct rump_sockaddr *baddr, uint32_t len)
{

	return;
}

static struct msghdr *
translate_msghdr(const struct rump_msghdr *bmsg, int *flags)
{

	return (struct msghdr *)__UNCONST(bmsg);
}

static void
translate_msghdr_back(const struct msghdr *lmsg, struct rump_msghdr *bmsg)
{

	return;
}

#else
static struct sockaddr *
translate_sockaddr(const struct sockaddr *addr, uint32_t len)
{
	struct sockaddr *laddr;
	const struct rump_sockaddr *baddr;

	baddr = (const struct rump_sockaddr *)addr;
	laddr = malloc(len);
	if (laddr == NULL)
		return NULL;
	memcpy(laddr, baddr, len);
	laddr->sa_family = translate_domain(baddr->sa_family);
	/* No sa_len for Linux and SunOS */
#if defined(__DragonFly__) || defined(__FreeBSD__) || defined(__OpenBSD__)
	laddr->sa_len = len;
#endif
	return laddr;
}

#define translate_back(_a_) case _a_: return RUMP_##_a_
static int translate_domain_back(int);
static int
translate_domain_back(int domain)
{

	switch (domain) {
	translate_back(AF_INET);
	translate_back(AF_INET6);
	default: return RUMP_AF_UNSPEC;
	}
}
#undef translate_back

static void
translate_sockaddr_back(const struct sockaddr *laddr,
		struct rump_sockaddr *baddr,
		uint32_t len)
{

	if (baddr != NULL) {
		memcpy(baddr, laddr, len);
		baddr->sa_family = translate_domain_back(laddr->sa_family);
		baddr->sa_len = len;
	}
	free(__UNCONST(laddr));
}

static struct msghdr *
translate_msghdr(const struct rump_msghdr *bmsg, int *flags)
{
	struct msghdr *rv;

	*flags = bsd_to_native_msg_flags(*flags);
	if (*flags < 0)
		*flags = 0;

	rv = malloc(sizeof(*rv));
	rv->msg_namelen = bmsg->msg_namelen;
	rv->msg_iov = bmsg->msg_iov;
	rv->msg_iovlen = bmsg->msg_iovlen;
	rv->msg_control = bmsg->msg_control;
	rv->msg_controllen = bmsg->msg_controllen;
	rv->msg_flags = 0;

	if (bmsg->msg_name != NULL) {
		rv->msg_name = translate_sockaddr(bmsg->msg_name,
				bmsg->msg_namelen);
		if (rv->msg_name == NULL) {
			free(rv);
			return NULL;
		}
	} else
		rv->msg_name = NULL;
	return rv;
}

static void
translate_msghdr_back(const struct msghdr *lmsg, struct rump_msghdr *bmsg)
{

	if (bmsg == NULL) {
		if (lmsg->msg_name != NULL)
			free(lmsg->msg_name);
		free(__UNCONST(lmsg));
		return;
	}
	bmsg->msg_namelen = lmsg->msg_namelen;
	bmsg->msg_iov = lmsg->msg_iov;
	bmsg->msg_iovlen = lmsg->msg_iovlen;
	bmsg->msg_control = lmsg->msg_control;
	bmsg->msg_controllen = lmsg->msg_controllen;
	bmsg->msg_flags = native_to_bsd_msg_flags(lmsg->msg_flags);

	if (lmsg->msg_name != NULL)
		translate_sockaddr_back(lmsg->msg_name, bmsg->msg_name,
				bmsg->msg_namelen);
	else
		bmsg->msg_name = NULL;

	free(__UNCONST(lmsg));
}
#endif

int
rumpcomp_sockin_socket(int domain, int type, int proto, int *s)
{
	void *cookie;
	int rv;

	domain = translate_domain(domain);

	cookie = rumpuser_component_unschedule();
	*s = socket(domain, type, proto);
	seterror(*s);
	rumpuser_component_schedule(cookie);

	return rumpuser_component_errtrans(rv);
}

int
rumpcomp_sockin_sendmsg(int s, const struct msghdr *msg, int flags, size_t *snd)
{
	void *cookie;
	ssize_t nn;
	int rv;

	msg = translate_msghdr((struct rump_msghdr *)msg, &flags);

	cookie = rumpuser_component_unschedule();
	nn = sendmsg(s, msg, flags);
	seterror(nn);
	*snd = (size_t)nn;
	rumpuser_component_schedule(cookie);

	translate_msghdr_back(msg, NULL);

	return rumpuser_component_errtrans(rv);
}

int
rumpcomp_sockin_recvmsg(int s, struct msghdr *msg, int flags, size_t *rcv)
{
	void *cookie;
	ssize_t nn;
	int rv;
	struct rump_msghdr *saveptr;

	saveptr = (struct rump_msghdr *)msg;
	msg = translate_msghdr(saveptr, &flags);

	cookie = rumpuser_component_unschedule();
	nn = recvmsg(s, msg, flags);
	seterror(nn);
	*rcv = (size_t)nn;
	rumpuser_component_schedule(cookie);

	translate_msghdr_back(msg, saveptr);

	return rumpuser_component_errtrans(rv);
}

int
rumpcomp_sockin_connect(int s, const struct sockaddr *name, int len)
{
	void *cookie;
	int rv;

	name = translate_sockaddr(name, len);

	cookie = rumpuser_component_unschedule();
	rv = connect(s, name, (socklen_t)len);
	seterror(rv);
	rumpuser_component_schedule(cookie);

	translate_sockaddr_back(name, NULL, len);

	return rumpuser_component_errtrans(rv);
}

int
rumpcomp_sockin_bind(int s, const struct sockaddr *name, int len)
{
	void *cookie;
	int rv;

	name = translate_sockaddr(name, len);

	cookie = rumpuser_component_unschedule();
	rv = bind(s, name, (socklen_t)len);
	seterror(rv);
	rumpuser_component_schedule(cookie);

	translate_sockaddr_back(name, NULL, len);

	return rumpuser_component_errtrans(rv);
}

int
rumpcomp_sockin_accept(int s, struct sockaddr *name, int *lenp, int *s2)
{
	void *cookie;
	int rv;
	struct rump_sockaddr *saveptr;

	saveptr = (struct rump_sockaddr *)name;
	name = translate_sockaddr(name, *lenp);

	cookie = rumpuser_component_unschedule();
	*s2 = accept(s, name, (socklen_t *)lenp);
	seterror(*s2);
	rumpuser_component_schedule(cookie);

	translate_sockaddr_back(name, saveptr, *lenp);

	return rumpuser_component_errtrans(rv);
}

int
rumpcomp_sockin_listen(int s, int backlog)
{
	void *cookie;
	int rv;

	cookie = rumpuser_component_unschedule();
	rv = listen(s, backlog);
	seterror(rv);
	rumpuser_component_schedule(cookie);

	return rumpuser_component_errtrans(rv);
}

int
rumpcomp_sockin_getname(int s, struct sockaddr *so, int *lenp,
	enum rumpcomp_sockin_getnametype which)
{
	socklen_t slen = *lenp;
	int rv;
	struct rump_sockaddr *saveptr;

	saveptr = (struct rump_sockaddr *)so;
	so = translate_sockaddr(so, *lenp);

	if (which == RUMPCOMP_SOCKIN_SOCKNAME)
		rv = getsockname(s, so, &slen);
	else
		rv = getpeername(s, so, &slen);

	seterror(rv);
	translate_sockaddr_back(so, saveptr, *lenp);

	*lenp = slen;

	return rumpuser_component_errtrans(rv);
}

int
rumpcomp_sockin_setsockopt(int s, int level, int name,
	const void *data, int dlen)
{
	socklen_t slen = dlen;
	int rv;

	translate_sockopt(&level, &name);
	if (level == -1 || name == -1) {
#ifdef SETSOCKOPT_STRICT
		errno = EINVAL;
		rv = -1;
#else
		rv = 0;
#endif
	} else
		rv = setsockopt(s, level, name, data, slen);

	seterror(rv);

	return rumpuser_component_errtrans(rv);
}

int
rumpcomp_sockin_poll(struct pollfd *fds, int nfds, int timeout, int *nready)
{
	void *cookie;
	int rv;

	cookie = rumpuser_component_unschedule();
	*nready = poll(fds, (nfds_t)nfds, timeout);
	seterror(*nready);
	rumpuser_component_schedule(cookie);

	return rumpuser_component_errtrans(rv);
}
#endif
