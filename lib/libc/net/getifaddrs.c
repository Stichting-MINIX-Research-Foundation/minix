/*	$NetBSD: getifaddrs.c,v 1.15 2012/03/13 21:13:40 christos Exp $	*/

/*
 * Copyright (c) 1995, 1999
 *	Berkeley Software Design, Inc.  All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 *
 * THIS SOFTWARE IS PROVIDED BY Berkeley Software Design, Inc. ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL Berkeley Software Design, Inc. BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 *
 *	BSDI getifaddrs.c,v 2.12 2000/02/23 14:51:59 dab Exp
 */

#include <sys/cdefs.h>
#if defined(LIBC_SCCS) && !defined(lint)
__RCSID("$NetBSD: getifaddrs.c,v 1.15 2012/03/13 21:13:40 christos Exp $");
#endif /* LIBC_SCCS and not lint */

#ifndef RUMP_ACTION
#include "namespace.h"
#endif
#include <sys/types.h>
#include <sys/ioctl.h>
#include <sys/socket.h>
#include <net/if.h>
#include <sys/param.h>
#include <net/route.h>
#include <sys/sysctl.h>
#include <net/if_dl.h>

#include <assert.h>
#include <errno.h>
#include <ifaddrs.h>
#include <stdlib.h>
#include <string.h>

#if defined(__weak_alias) && !defined(RUMP_ACTION)
__weak_alias(getifaddrs,_getifaddrs)
__weak_alias(freeifaddrs,_freeifaddrs)
#endif

#ifdef RUMP_ACTION
#include <rump/rump_syscalls.h>
#define sysctl(a,b,c,d,e,f) rump_sys___sysctl(a,b,c,d,e,f)
#endif

#define	SA_RLEN(sa)	RT_ROUNDUP((sa)->sa_len)

int
getifaddrs(struct ifaddrs **pif)
{
	size_t icnt = 1;
	size_t dcnt = 0;
	size_t ncnt = 0;
	static const int mib[] = {
		CTL_NET,
		PF_ROUTE,
		0,			/* protocol */
		0,			/* wildcard address family */
		NET_RT_IFLIST,
		0			/* no flags */
	};
	size_t needed;
	char *buf;
	char *next;
	struct ifaddrs cif;
	char *p, *p0;
	struct rt_msghdr *rtm;
	struct if_msghdr *ifm;
	struct ifa_msghdr *ifam;
	struct sockaddr *sa;
	struct ifaddrs *ifa, *ift;
	u_short idx = 0;
	int i;
	size_t len, alen;
	char *data;
	char *names;

	_DIAGASSERT(pif != NULL);

	if (sysctl(mib, (u_int)__arraycount(mib), NULL, &needed, NULL, 0) < 0)
		return (-1);
	if ((buf = malloc(needed)) == NULL)
		return (-1);
	if (sysctl(mib, (u_int)__arraycount(mib), buf, &needed, NULL, 0) < 0) {
		free(buf);
		return (-1);
	}

	for (next = buf; next < buf + needed; next += rtm->rtm_msglen) {
		rtm = (struct rt_msghdr *)(void *)next;
		if (rtm->rtm_version != RTM_VERSION)
			continue;
		switch (rtm->rtm_type) {
		case RTM_IFINFO:
			ifm = (struct if_msghdr *)(void *)rtm;
			if (ifm->ifm_addrs & RTA_IFP) {
				const struct sockaddr_dl *dl;

				idx = ifm->ifm_index;
				++icnt;
				dl = (struct sockaddr_dl *)(void *)(ifm + 1);
				dcnt += SA_RLEN((const struct sockaddr *)(const void *)dl) +
				    ALIGNBYTES;
				dcnt += sizeof(ifm->ifm_data);
				ncnt += dl->sdl_nlen + 1;
			} else
				idx = 0;
			break;

		case RTM_NEWADDR:
			ifam = (struct ifa_msghdr *)(void *)rtm;
			if (idx && ifam->ifam_index != idx)
				abort();	/* this cannot happen */

#define	RTA_MASKS	(RTA_NETMASK | RTA_IFA | RTA_BRD)
			if (idx == 0 || (ifam->ifam_addrs & RTA_MASKS) == 0)
				break;
			p = (char *)(void *)(ifam + 1);
			++icnt;
			/* Scan to look for length of address */
			alen = 0;
			for (p0 = p, i = 0; i < RTAX_MAX; i++) {
				if ((RTA_MASKS & ifam->ifam_addrs & (1 << i))
				    == 0)
					continue;
				sa = (struct sockaddr *)(void *)p;
				len = SA_RLEN(sa);
				if (i == RTAX_IFA) {
					alen = len;
					break;
				}
				p += len;
			}
			for (p = p0, i = 0; i < RTAX_MAX; i++) {
				if ((RTA_MASKS & ifam->ifam_addrs & (1 << i))
				    == 0)
					continue;
				sa = (struct sockaddr *)(void *)p;
				len = SA_RLEN(sa);
				if (i == RTAX_NETMASK && sa->sa_len == 0)
					dcnt += alen;
				else
					dcnt += len;
				p += len;
			}
			break;
		}
	}

	if (icnt + dcnt + ncnt == 1) {
		*pif = NULL;
		free(buf);
		return (0);
	}
	data = malloc(sizeof(struct ifaddrs) * icnt + dcnt + ncnt);
	if (data == NULL) {
		free(buf);
		return(-1);
	}

	ifa = (struct ifaddrs *)(void *)data;
	data += sizeof(struct ifaddrs) * icnt;
	names = data + dcnt;

	memset(ifa, 0, sizeof(struct ifaddrs) * icnt);
	ift = ifa;

	idx = 0;
	for (next = buf; next < buf + needed; next += rtm->rtm_msglen) {
		rtm = (struct rt_msghdr *)(void *)next;
		if (rtm->rtm_version != RTM_VERSION)
			continue;
		switch (rtm->rtm_type) {
		case RTM_IFINFO:
			ifm = (struct if_msghdr *)(void *)rtm;
			if (ifm->ifm_addrs & RTA_IFP) {
				const struct sockaddr_dl *dl;

				idx = ifm->ifm_index;
				dl = (struct sockaddr_dl *)(void *)(ifm + 1);

				memset(&cif, 0, sizeof(cif));

				cif.ifa_name = names;
				cif.ifa_flags = (int)ifm->ifm_flags;
				memcpy(names, dl->sdl_data,
				    (size_t)dl->sdl_nlen);
				names[dl->sdl_nlen] = 0;
				names += dl->sdl_nlen + 1;

				cif.ifa_addr = (struct sockaddr *)(void *)data;
				memcpy(data, dl, (size_t)dl->sdl_len);
				data += SA_RLEN((const struct sockaddr *)(const void *)dl);

				/* ifm_data needs to be aligned */
				cif.ifa_data = data = (void *)ALIGN(data);
				memcpy(data, &ifm->ifm_data, sizeof(ifm->ifm_data));
 				data += sizeof(ifm->ifm_data);
			} else
				idx = 0;
			break;

		case RTM_NEWADDR:
			ifam = (struct ifa_msghdr *)(void *)rtm;
			if (idx && ifam->ifam_index != idx)
				abort();	/* this cannot happen */

			if (idx == 0 || (ifam->ifam_addrs & RTA_MASKS) == 0)
				break;
			ift->ifa_name = cif.ifa_name;
			ift->ifa_flags = cif.ifa_flags;
			ift->ifa_data = NULL;
			p = (char *)(void *)(ifam + 1);
			/* Scan to look for length of address */
			alen = 0;
			for (p0 = p, i = 0; i < RTAX_MAX; i++) {
				if ((RTA_MASKS & ifam->ifam_addrs & (1 << i))
				    == 0)
					continue;
				sa = (struct sockaddr *)(void *)p;
				len = SA_RLEN(sa);
				if (i == RTAX_IFA) {
					alen = len;
					break;
				}
				p += len;
			}
			for (p = p0, i = 0; i < RTAX_MAX; i++) {
				if ((RTA_MASKS & ifam->ifam_addrs & (1 << i))
				    == 0)
					continue;
				sa = (struct sockaddr *)(void *)p;
				len = SA_RLEN(sa);
				switch (i) {
				case RTAX_IFA:
					ift->ifa_addr =
					    (struct sockaddr *)(void *)data;
					memcpy(data, p, len);
					data += len;
					if (ift->ifa_addr->sa_family == AF_LINK)
						ift->ifa_data = cif.ifa_data;
					break;

				case RTAX_NETMASK:
					ift->ifa_netmask =
					    (struct sockaddr *)(void *)data;
					if (sa->sa_len == 0) {
						memset(data, 0, alen);
						data += alen;
						break;
					}
					memcpy(data, p, len);
					data += len;
					break;

				case RTAX_BRD:
					ift->ifa_broadaddr =
					    (struct sockaddr *)(void *)data;
					memcpy(data, p, len);
					data += len;
					break;
				}
				p += len;
			}


			ift = (ift->ifa_next = ift + 1);
			break;
		}
	}

	free(buf);
	if (--ift >= ifa) {
		ift->ifa_next = NULL;
		*pif = ifa;
	} else {
		*pif = NULL;
		free(ifa);
	}
	return (0);
}

void
freeifaddrs(struct ifaddrs *ifp)
{

	_DIAGASSERT(ifp != NULL);

	free(ifp);
}
