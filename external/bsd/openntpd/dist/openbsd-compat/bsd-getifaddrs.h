/* $Id: bsd-getifaddrs.h,v 1.1 2005/07/06 07:53:50 dtucker Exp $ */

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

#ifndef HAVE_GETIFADDRS
# ifndef _BSD_GETIFADDRS_H
# define _BSD_GETIFADDRS_H

#ifdef ifa_broadaddr
#undef ifa_broadaddr
#endif

#ifdef ifa_dstaddr
#undef ifa_dstaddr
#endif

struct ifaddrs {
         struct ifaddrs   *ifa_next;         /* Pointer to next struct */
         char             *ifa_name;         /* Interface name */
         u_int             ifa_flags;        /* Interface flags */
         struct sockaddr  *ifa_addr;         /* Interface address */
         struct sockaddr  *ifa_netmask;      /* Interface netmask */
         struct sockaddr  *ifa_broadaddr;    /* Interface broadcast address */
         struct sockaddr  *ifa_dstaddr;      /* P2P interface destination */
         void             *ifa_data;         /* Address specific data */
};

int	getifaddrs(struct ifaddrs **);
void	freeifaddrs(struct ifaddrs *);

# endif /* _BSD_GETIFADDRS_H */
#endif /* HAVE_GETIFADDRS */
