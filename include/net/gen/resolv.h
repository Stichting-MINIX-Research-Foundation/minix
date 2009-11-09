/*
 * Copyright (c) 1983, 1987, 1989 The Regents of the University of California.
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
 *	@(#)resolv.h	5.10 (Berkeley) 6/1/90
 */
#ifndef _NET__GEN__RESOLV_H
#define _NET__GEN__RESOLV_H

/*
 * Resolver configuration file.
 * Normally not present, but may contain the address of the
 * inital name server(s) to query and the domain search list.
 */

#ifndef _PATH_RESCONF
#define _PATH_RESCONF        "/etc/resolv.conf"
#endif

/*
 * Global defines and variables for resolver stub.
 */
#define	MAXNS		3		/* max # name servers we'll track */
#define	MAXDFLSRCH	3		/* # default domain levels to try */
#define	MAXDNSRCH	6		/* max # domains in search path */
#define	LOCALDOMAINPARTS 2		/* min levels in name that is "local" */

#define	RES_TIMEOUT	5		/* min. seconds between retries */

#define NAMESERVER_PORT	53

struct state {
	int	retrans;	 	/* retransmition time interval */
	int	retry;			/* number of times to retransmit */
	long	options;		/* option flags - see below. */
	int	nscount;		/* number of name servers */
	ipaddr_t nsaddr_list[MAXNS];	/* address of name server */
#define	nsaddr	nsaddr_list[0]		/* for backward compatibility */
	u16_t	nsport_list[MAXNS];	/* port of name server */
	u16_t	id;			/* current packet id */
	char	defdname[MAXDNAME];	/* default domain */
	char	*dnsrch[MAXDNSRCH+1];	/* components of domain to search */
};

/*
 * Resolver options
 */
#define RES_INIT	0x0001		/* address initialized */
#define RES_DEBUG	0x0002		/* print debug messages */
#define RES_AAONLY	0x0004		/* authoritative answers only */
#define RES_USEVC	0x0008		/* use virtual circuit */
#define RES_PRIMARY	0x0010		/* query primary server only */
#define RES_IGNTC	0x0020		/* ignore trucation errors */
#define RES_RECURSE	0x0040		/* recursion desired */
#define RES_DEFNAMES	0x0080		/* use default domain name */
#define RES_STAYOPEN	0x0100		/* Keep TCP socket open */
#define RES_DNSRCH	0x0200		/* search up local domain tree */

#define RES_DEFAULT	(RES_RECURSE | RES_DEFNAMES | RES_DNSRCH )

extern struct state _res;

struct rrec;

int res_init _ARGS(( void ));
int res_mkquery _ARGS(( int op, const char *dname, int class, int type,
	const char *data, int datalen, const struct rrec *newrr,
	char *buf, int buflen ));
int res_query _ARGS(( char *name, int class, int type, u8_t *answer, 
	int anslen ));
int res_querydomain _ARGS(( char *name, char *domain, int class, int type, 
	u8_t *answer, int anslen ));
int res_search _ARGS(( char *name, int class, int type, u8_t *answer, 
	int anslen ));
int res_send _ARGS(( const char *buf, int buflen, char *answer, int anslen ));
void _res_close _ARGS(( void ));

int dn_comp _ARGS(( const u8_t *exp_dn, u8_t *comp_dn, int length, 
	u8_t **dnptrs, u8_t **lastdnptr ));
int dn_expand  _ARGS(( const u8_t *msg, const u8_t *eomorig,
	const u8_t *comp_dn, u8_t *exp_dn, int length ));
int dn_skipname _ARGS(( const u8_t *comp_dn, const u8_t *eom ));

char *__hostalias _ARGS(( const char *name ));

u16_t _getshort _ARGS(( const u8_t *msgp ));
u32_t _getlong _ARGS(( const u8_t *msgp ));
void __putshort _ARGS(( U16_t s, u8_t *msgp ));
void __putlong _ARGS(( u32_t l, u8_t *msgp ));

void p_query _ARGS(( char *msg ));

#endif /* _NET__GEN__RESOLV_H */
