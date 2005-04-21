/*
 * Copyright (c) 1983, 1989 Regents of the University of California.
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
 *
 *	@(#)nameser.h	5.24 (Berkeley) 6/1/90
 */

/*
server/ip/gen/nameser.h

Created Sept 18, 1991 by Philip Homburg
*/

#ifndef __SERVER__IP__GEN__NAEMSER_H__
#define __SERVER__IP__GEN__NAEMSER_H__

typedef struct dns_hdr
{
	u16_t dh_id;
	u8_t dh_flag1;
	u8_t dh_flag2;
	u16_t dh_qdcount;
	u16_t dh_ancount;
	u16_t dh_nscount;
	u16_t dh_arcount;
} dns_hdr_t;

#define DHF_QR		0x80
#define DHF_OPCODE	0x78
#define DHF_AA		0x04
#define DHF_TC		0x02
#define DHF_RD		0x01

#define DHF_RA		0x80
#define DHF_PR		0x40
#define DHF_UNUSED	0x30
#define DHF_RCODE	0x0F


/*
Define constants based on rfc883
*/
#define PACKETSZ	512		/* maximum packet size */
#define MAXDNAME	256		/* maximum domain name */
#define MAXCDNAME	255		/* maximum compressed domain name */
#define MAXLABEL	63		/* maximum length of domain label */
	/* Number of bytes of fixed size data in query structure */
#define QFIXEDSZ	4
	/* number of bytes of fixed size data in resource record */
#define RRFIXEDSZ	10
#define INDIR_MASK	0xc0
			/* Defines for handling compressed domain names */

/*
Opcodes for DNS
*/

#define QUERY		0x0			/* standard query */
#define IQUERY		0x1			/* inverse query */

/*
Error codes
*/
#define NOERROR		0			/* no error */
#define FORMERR		1			/* format error */
#define SERVFAIL	2			/* server failure */
#define NXDOMAIN	3			/* non existent domain */
#define NOTIMP		4			/* not implemented */
#define REFUSED		5			/* query refused */
	/* non standard */
#define NOCHANGE	0xf			/* update failed to change db */

/* Valid types */

#define T_A		1		/* host address */
#define T_NS		2		/* authoritative server */
#define T_MD		3		/* mail destination */
#define T_MF		4		/* mail forwarder */
#define T_CNAME		5		/* connonical name */
#define T_SOA		6		/* start of authority zone */
#define T_MB		7		/* mailbox domain name */
#define T_MG		8		/* mail group member */
#define T_MR		9		/* mail rename name */
#define T_NULL		10		/* null resource record */
#define T_WKS		11		/* well known service */
#define T_PTR		12		/* domain name pointer */
#define T_HINFO		13		/* host information */
#define T_MINFO		14		/* mailbox information */
#define T_MX		15		/* mail routing information */
#define T_TXT		16		/* text strings */
	/* non standard */
#define T_UINFO		100		/* user (finger) information */
#define T_UID		101		/* user ID */
#define T_GID		102		/* group ID */
#define T_UNSPEC	103		/* Unspecified format (binary data) */
	/* Query type values which do not appear in resource records */
#define T_AXFR		252		/* transfer zone of authority */
#define T_MAILB		253		/* transfer mailbox records */
#define T_MAILA		254		/* transfer mail agent records */
#define T_ANY		255		/* wildcard match */

/* Valid classes */

#define C_IN		1			/* the internet */
#define C_CHAOS		3			/* for chaos net (MIT) */
#define C_HS		4		/* for Hesiod name server at MIT */

#define C_ANY		255			/* wildcard */

#endif /* __SERVER__IP__GEN__NAEMSER_H__ */
