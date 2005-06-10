/*
 * Copyright (c) 1985 Regents of the University of California.
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms are permitted
 * provided that: (1) source distributions retain this entire copyright
 * notice and comment, and (2) distributions including binaries display
 * the following acknowledgement:  ``This product includes software
 * developed by the University of California, Berkeley and its contributors''
 * in the documentation or other materials provided with the distribution
 * and in all advertising materials mentioning features or use of this
 * software. Neither the name of the University nor the names of its
 * contributors may be used to endorse or promote products derived
 * from this software without specific prior written permission.
 * THIS SOFTWARE IS PROVIDED ``AS IS'' AND WITHOUT ANY EXPRESS OR
 * IMPLIED WARRANTIES, INCLUDING, WITHOUT LIMITATION, THE IMPLIED
 * WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE.
 */

#if defined(LIBC_SCCS) && !defined(lint)
static char sccsid[] = "@(#)res_mkquery.c	6.12 (Berkeley) 6/1/90";
#endif /* LIBC_SCCS and not lint */

#if _MINIX
#include <sys/types.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <net/hton.h>
#include <net/gen/in.h>
#include <net/gen/nameser.h>
#include <net/gen/resolv.h>

#define bzero(b,l) memset(b,0,l)
#define bcopy(s,d,l) memcpy(d,s,l)

#define putshort __putshort
#define putlong __putlong
#else
#include <stdio.h>
#include <sys/types.h>
#include <netinet/in.h>
#include <arpa/nameser.h>
#include <resolv.h>
#endif

#ifdef __STDC__
#define _CONST	const
#else
#define _CONST
#endif

/*
 * Form all types of queries.
 * Returns the size of the result or -1.
 */
res_mkquery(op, dname, class, type, data, datalen, newrr, buf, buflen)
	int op;			/* opcode of query */
	_CONST char *dname;	/* domain name */
	int class, type;	/* class and type of query */
	_CONST char *data;	/* resource record data */
	int datalen;		/* length of data */
	_CONST struct rrec *newrr; /* new rr for modify or append */
	char *buf;		/* buffer to put query */
	int buflen;		/* size of buffer */
{
	register dns_hdr_t *hp;
	register char *cp;
	register int n;
	char *dnptrs[10], **dpp, **lastdnptr;

#ifdef DEBUG
	if (_res.options & RES_DEBUG)
		printf("res_mkquery(%d, %s, %d, %d)\n", op, dname, class, type);
#endif /* DEBUG */
	/*
	 * Initialize header fields.
	 */
	if ((buf == NULL) || (buflen < sizeof(dns_hdr_t)))
		return(-1);
	bzero(buf, sizeof(dns_hdr_t));
	hp = (dns_hdr_t *) buf;
	hp->dh_id = htons(++_res.id);
	hp->dh_flag1= 0;
	hp->dh_flag2= 0;
	hp->dh_flag1 |= (op << 3) & DHF_OPCODE;
	hp->dh_flag2 |= ((_res.options & RES_PRIMARY) != 0 ? 1 : 0) << 6;
	hp->dh_flag1 |= (_res.options & RES_RECURSE) != 0 ? 1 : 0;
	hp->dh_flag2 |= NOERROR & DHF_RCODE;
	cp = buf + sizeof(dns_hdr_t);
	buflen -= sizeof(dns_hdr_t);
	dpp = dnptrs;
	*dpp++ = buf;
	*dpp++ = NULL;
	lastdnptr = dnptrs + sizeof(dnptrs)/sizeof(dnptrs[0]);
	/*
	 * perform opcode specific processing
	 */
	switch (op) {
	case QUERY:
		if ((buflen -= QFIXEDSZ) < 0)
			return(-1);
		if ((n = dn_comp((u8_t *)dname, (u8_t *)cp, buflen, 
			(u8_t **)dnptrs, (u8_t **)lastdnptr)) < 0)
			return (-1);
		cp += n;
		buflen -= n;
		putshort(type, (u8_t *)cp);
		cp += sizeof(u_short);
		putshort(class, (u8_t *)cp);
		cp += sizeof(u_short);
		hp->dh_qdcount = HTONS(1);
		if (op == QUERY || data == NULL)
			break;
		/*
		 * Make an additional record for completion domain.
		 */
		buflen -= RRFIXEDSZ;
		if ((n = dn_comp((u8_t *)data, (u8_t *)cp, buflen, 
			(u8_t **)dnptrs, (u8_t **)lastdnptr)) < 0)
			return (-1);
		cp += n;
		buflen -= n;
		putshort(T_NULL, (u8_t *)cp);
		cp += sizeof(u_short);
		putshort(class, (u8_t *)cp);
		cp += sizeof(u_short);
		putlong(0, (u8_t *)cp);
		cp += sizeof(u_long);
		putshort(0, (u8_t *)cp);
		cp += sizeof(u_short);
		hp->dh_arcount = HTONS(1);
		break;

	case IQUERY:
		/*
		 * Initialize answer section
		 */
		if (buflen < 1 + RRFIXEDSZ + datalen)
			return (-1);
		*cp++ = '\0';	/* no domain name */
		putshort(type, (u8_t *)cp);
		cp += sizeof(u_short);
		putshort(class, (u8_t *)cp);
		cp += sizeof(u_short);
		putlong(0, (u8_t *)cp);
		cp += sizeof(u_long);
		putshort(datalen, (u8_t *)cp);
		cp += sizeof(u_short);
		if (datalen) {
			bcopy(data, cp, datalen);
			cp += datalen;
		}
		hp->dh_ancount = HTONS(1);
		break;

#ifdef ALLOW_UPDATES
	/*
	 * For UPDATEM/UPDATEMA, do UPDATED/UPDATEDA followed by UPDATEA
	 * (Record to be modified is followed by its replacement in msg.)
	 */
	case UPDATEM:
	case UPDATEMA:

	case UPDATED:
		/*
		 * The res code for UPDATED and UPDATEDA is the same; user
		 * calls them differently: specifies data for UPDATED; server
		 * ignores data if specified for UPDATEDA.
		 */
	case UPDATEDA:
		buflen -= RRFIXEDSZ + datalen;
		if ((n = dn_comp(dname, cp, buflen, dnptrs, lastdnptr)) < 0)
			return (-1);
		cp += n;
		putshort(type, cp);
                cp += sizeof(u_short);
                putshort(class, cp);
                cp += sizeof(u_short);
		putlong(0, cp);
		cp += sizeof(u_long);
		putshort(datalen, cp);
                cp += sizeof(u_short);
		if (datalen) {
			bcopy(data, cp, datalen);
			cp += datalen;
		}
		if ( (op == UPDATED) || (op == UPDATEDA) ) {
			hp->ancount = HTONS(0);
			break;
		}
		/* Else UPDATEM/UPDATEMA, so drop into code for UPDATEA */

	case UPDATEA:	/* Add new resource record */
		buflen -= RRFIXEDSZ + datalen;
		if ((n = dn_comp(dname, cp, buflen, dnptrs, lastdnptr)) < 0)
			return (-1);
		cp += n;
		putshort(newrr->r_type, cp);
                cp += sizeof(u_short);
                putshort(newrr->r_class, cp);
                cp += sizeof(u_short);
		putlong(0, cp);
		cp += sizeof(u_long);
		putshort(newrr->r_size, cp);
                cp += sizeof(u_short);
		if (newrr->r_size) {
			bcopy(newrr->r_data, cp, newrr->r_size);
			cp += newrr->r_size;
		}
		hp->ancount = HTONS(0);
		break;

#endif /* ALLOW_UPDATES */
	}
	return (cp - buf);
}
