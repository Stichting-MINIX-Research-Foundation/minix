/* $Id: bsd-getifaddrs.c,v 1.1 2005/07/06 07:53:50 dtucker Exp $ */

/*
 * A minimal getifaddrs replacement for OpenNTPD
 * This only implements the components that ntpd uses (ie ifa_addr for
 * INET and INET6 interfaces).  It is not suitable for any other purpose.
 */

/*
 * Copyright (c) 2005 Darren Tucker <dtucker at zip com au>
 *
 * Permission to use, copy, modify, and distribute this software for any
 * purpose with or without fee is hereby granted, provided that the above
 * copyright notice and this permission notice appear in all copies.
 *
 * THE SOFTWARE IS PROVIDED "AS IS" AND THE AUTHOR DISCLAIMS ALL WARRANTIES
 * WITH REGARD TO THIS SOFTWARE INCLUDING ALL IMPLIED WARRANTIES OF
 * MERCHANTABILITY AND FITNESS. IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR
 * ANY SPECIAL, DIRECT, INDIRECT, OR CONSEQUENTIAL DAMAGES OR ANY DAMAGES
 * WHATSOEVER RESULTING FROM LOSS OF MIND, USE, DATA OR PROFITS, WHETHER
 * IN AN ACTION OF CONTRACT, NEGLIGENCE OR OTHER TORTIOUS ACTION, ARISING
 * OUT OF OR IN CONNECTION WITH THE USE OR PERFORMANCE OF THIS SOFTWARE.
 */

#include "includes.h"

#ifndef HAVE_GETIFADDRS

#include <sys/types.h>
#include <sys/ioctl.h>
#include <sys/socket.h>
#include <net/if.h>
#include <netinet/in.h>
#include <errno.h>

#ifdef HAVE_SYS_SOCKIO_H
#include <sys/sockio.h>
#endif

#define MAX_INTERFACES	128
#define STEPSZ		8

# if !defined(IF_NAMESIZE) && defined(IFNAMSIZ)
#  define IF_NAMESIZE IFNAMSIZ
# endif

# ifdef GETIFADDRS_VIA_SIOCGIFCONF
static void *
xrealloc(void *old, size_t size) {
	if (old == NULL)
		return malloc(size);
	else
		return realloc(old, size);
}

static void
add_interface(struct ifaddrs **ifa, struct ifreq *ifr, struct sockaddr *sa)
{
	struct ifaddrs *ifa_new = NULL;
	size_t sasize;
	int family = ifr->ifr_addr.sa_family;

	if (family != AF_INET && family != AF_INET6)
		return;

	sasize = SA_LEN(sa);

	if ((ifa_new = malloc(sizeof(*ifa_new))) == NULL)
		goto fail;
	memset(ifa_new, 0, sizeof(*ifa_new));

	if ((ifa_new->ifa_name = strdup(ifr->ifr_name)) == NULL)
		goto fail;
	if ((ifa_new->ifa_addr = malloc(sasize)) == NULL)
		goto fail;
	if (memcpy(ifa_new->ifa_addr, sa, sasize) == NULL)
		goto fail;
	ifa_new->ifa_next = *ifa;
	*ifa = ifa_new;
	return;

fail:
	if (ifa_new->ifa_name != NULL)
		free(ifa_new->ifa_name);
	if (ifa_new->ifa_addr != NULL)
		free(ifa_new->ifa_addr);
	if (ifa_new != NULL)
		free(ifa_new);
}
# endif

int
getifaddrs(struct ifaddrs **ifa)
{
# ifdef GETIFADDRS_VIA_SIOCGIFCONF
	int fd, i, oldlen = 0;
	size_t buflen = 0;
	char *oldbuf = NULL, *buf = NULL;
	struct ifconf ifc;
	struct ifreq *ifr;
	struct sockaddr *sa;

	if ((fd = socket(PF_INET, SOCK_DGRAM, 0)) == -1)
		return -1;

	*ifa = NULL;
	/*
	 * We iterate until the buffer returned by the ioctl stops growing.
	 * Yes, this is ugly.
	 */
	for (i = STEPSZ; i < MAX_INTERFACES; i += STEPSZ) {
		buflen = i * sizeof(struct ifreq);
		oldbuf = buf;
		if ((buf = xrealloc(buf, buflen)) == NULL)
			goto fail;
		ifc.ifc_buf = buf;
		ifc.ifc_len = buflen;

		if (ioctl(fd, SIOCGIFCONF, &ifc) < 0)
			goto fail;
		if (oldlen == ifc.ifc_len)
			break;
		oldlen = ifc.ifc_len;
#  ifdef DEBUG_GETIFADDRS
		log_info("%s: i %d len %d oldlen %d", __func__, i, buflen,
		    oldlen);
#  endif
	}
	
	/* walk the device list, add the inet and inet6 ones to the list */
	for (oldbuf = buf; buf < oldbuf + ifc.ifc_len;) {
		ifr = (struct ifreq *)buf;
		sa = (struct sockaddr *)&ifr->ifr_addr;
#  if defined(HAVE_SOCKADDR_SA_LEN)
		buf += IF_NAMESIZE + SA_LEN(sa);
#  else
		buf += sizeof(struct ifreq);
#  endif
#  ifdef DEBUG_GETIFADDRS
		log_info("%s: if %s family %d", __func__, ifr->ifr_name,
		    ifr->ifr_addr.sa_family);
#  endif
		add_interface(ifa, ifr, sa);
	}
	return 0;

fail:
	if (oldbuf != NULL)
		free(oldbuf);
	if (*ifa != NULL) {
		freeifaddrs(*ifa);
		*ifa = NULL;
	}
	close(fd);
	return -1;
# else
	fprintf(stderr, "\"listen on *\" not supported on this "
	    "platform, interface address required\n");
	errno = ENOSYS;
	return -1;
# endif
}

void
freeifaddrs(struct ifaddrs *ifap)
{
# ifdef GETIFADDRS_VIA_SIOCGIFCONF
	struct ifaddrs *tmpifa = ifap;

	while (ifap != NULL) {
		if (ifap->ifa_name != NULL)
			free(ifap->ifa_name);
		if (ifap->ifa_addr != NULL)
			free(ifap->ifa_addr);
		tmpifa = ifap->ifa_next;
		free(ifap);
		ifap = tmpifa;
	}
	return;
# endif
}
#endif /* HAVE_GETIFADDRS */
