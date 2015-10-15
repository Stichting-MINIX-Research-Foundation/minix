/*	$NetBSD: ethers.c,v 1.25 2014/09/18 13:58:20 christos Exp $	*/

/* 
 * ethers(3N) a la Sun.
 *
 * Written by Roland McGrath <roland@frob.com> 10/14/93.
 * Public domain.
 */

#include <sys/cdefs.h>
#if defined(LIBC_SCCS) && !defined(lint)
__RCSID("$NetBSD: ethers.c,v 1.25 2014/09/18 13:58:20 christos Exp $");
#endif /* LIBC_SCCS and not lint */

#include "namespace.h"
#include <sys/param.h>
#include <sys/socket.h>

#include <net/if.h>
#include <net/if_ether.h>
#include <netinet/in.h>

#include <assert.h>
#include <errno.h>
#include <paths.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#ifdef YP
#include <rpcsvc/ypclnt.h>
#endif

#ifdef __weak_alias
__weak_alias(ether_aton,_ether_aton)
__weak_alias(ether_hostton,_ether_hostton)
__weak_alias(ether_line,_ether_line)
__weak_alias(ether_ntoa,_ether_ntoa)
__weak_alias(ether_ntohost,_ether_ntohost)
#endif

#ifndef _PATH_ETHERS
#define _PATH_ETHERS "/etc/ethers"
#endif

char *
ether_ntoa(const struct ether_addr *e)
{
	static char a[18];

	_DIAGASSERT(e != NULL);

	(void) snprintf(a, sizeof a, "%02x:%02x:%02x:%02x:%02x:%02x",
	    e->ether_addr_octet[0], e->ether_addr_octet[1],
	    e->ether_addr_octet[2], e->ether_addr_octet[3],
	    e->ether_addr_octet[4], e->ether_addr_octet[5]);
	return a;
}

struct ether_addr *
ether_aton(const char *s)
{
	static struct ether_addr n;
	u_int i[6];

	_DIAGASSERT(s != NULL);

	if (sscanf(s, " %x:%x:%x:%x:%x:%x ", &i[0], &i[1],
	    &i[2], &i[3], &i[4], &i[5]) == 6) {
		n.ether_addr_octet[0] = (u_char)i[0];
		n.ether_addr_octet[1] = (u_char)i[1];
		n.ether_addr_octet[2] = (u_char)i[2];
		n.ether_addr_octet[3] = (u_char)i[3];
		n.ether_addr_octet[4] = (u_char)i[4];
		n.ether_addr_octet[5] = (u_char)i[5];
		return &n;
	}
	return NULL;
}

int
ether_ntohost(char *hostname, const struct ether_addr *e)
{
	FILE *f; 
	char *p;
	struct ether_addr try;

	_DIAGASSERT(hostname != NULL);
	_DIAGASSERT(e != NULL);

#ifdef YP
	char trybuf[sizeof "xx:xx:xx:xx:xx:xx"];
	int trylen;
	trylen = snprintf(trybuf, sizeof trybuf, "%x:%x:%x:%x:%x:%x", 
	    e->ether_addr_octet[0], e->ether_addr_octet[1],
	    e->ether_addr_octet[2], e->ether_addr_octet[3],
	    e->ether_addr_octet[4], e->ether_addr_octet[5]);
#endif

	f = fopen(_PATH_ETHERS, "re");
	if (f == NULL)
		return -1;
	for (p = NULL;;) {
		free(p);
		p = fparseln(f, NULL, NULL, NULL, FPARSELN_UNESCALL);
		if (p == NULL)
			break;
#ifdef YP
		/* A + in the file means try YP now.  */
		if (strcmp(p, "+") == 0) {
			char *ypbuf, *ypdom;
			int ypbuflen;

			if (yp_get_default_domain(&ypdom))
				continue;
			if (yp_match(ypdom, "ethers.byaddr", trybuf,
			    trylen, &ypbuf, &ypbuflen))
				continue;
			ypbuflen = ether_line(ypbuf, &try, hostname);
			free(ypbuf);
			if (ypbuflen == 0)
				goto done;
			continue;
		}
#endif
		if (ether_line(p, &try, hostname) == 0 &&
		    memcmp(&try, e, sizeof try) == 0)
			goto done;
	}
	free(p);
	(void)fclose(f);
	errno = ENOENT;
	return -1;
done:
	free(p);
	(void)fclose(f);
	return 0;
}

int
ether_hostton(const char *hostname, struct ether_addr *e)
{
	FILE *f;
	char *p;
	char try[MAXHOSTNAMELEN + 1];
#ifdef YP
	int hostlen = (int)strlen(hostname);
#endif

	_DIAGASSERT(hostname != NULL);
	_DIAGASSERT(e != NULL);

	f = fopen(_PATH_ETHERS, "re");
	if (f == NULL)
		return -1;

	for (p = NULL;;) {
		free(p);
		p = fparseln(f, NULL, NULL, NULL, FPARSELN_UNESCALL);
		if (p == NULL)
			break;
#ifdef YP
		/* A + in the file means try YP now.  */
		if (strcmp(p, "+") == 0) {
			char *ypbuf, *ypdom;
			int ypbuflen;

			if (yp_get_default_domain(&ypdom))
				continue;
			if (yp_match(ypdom, "ethers.byname", hostname, hostlen,
			    &ypbuf, &ypbuflen))
				continue;
			ypbuflen = ether_line(ypbuf, e, try);
			free(ypbuf);
			if (ypbuflen == 0)
				goto done;
			continue;
		}
#endif
		if (ether_line(p, e, try) == 0 && strcmp(hostname, try) == 0)
			goto done;
	}
	free(p);
	(void)fclose(f);
	errno = ENOENT;
	return -1;
done:
	free(p);
	(void)fclose(f);
	return 0;
}

int
ether_line(const char *l, struct ether_addr *e, char *hostname)
{
	u_int i[6];

#define S2(arg) #arg
#define S1(arg) S2(arg)
	static const char fmt[] = " %x:%x:%x:%x:%x:%x"
	    " %" S1(MAXHOSTNAMELEN) "s\n";
#undef S2
#undef S1
	
	_DIAGASSERT(l != NULL);
	_DIAGASSERT(e != NULL);
	_DIAGASSERT(hostname != NULL);

	if (sscanf(l, fmt,
	    &i[0], &i[1], &i[2], &i[3], &i[4], &i[5], hostname) == 7) {
		e->ether_addr_octet[0] = (u_char)i[0];
		e->ether_addr_octet[1] = (u_char)i[1];
		e->ether_addr_octet[2] = (u_char)i[2];
		e->ether_addr_octet[3] = (u_char)i[3];
		e->ether_addr_octet[4] = (u_char)i[4];
		e->ether_addr_octet[5] = (u_char)i[5];
		return 0;
	}
	errno = EINVAL;
	return -1;
}
