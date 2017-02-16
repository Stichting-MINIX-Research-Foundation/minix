/*	$NetBSD: nslint.c,v 1.1.1.3 2014/12/10 03:34:34 christos Exp $	*/

/*
 * Copyright (c) 1992, 1993, 1994, 1995, 1996, 1997, 1998, 1999, 2000, 2001, 2002, 2003, 2005, 2006, 2007, 2008, 2009
 *	The Regents of the University of California.  All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that: (1) source code distributions
 * retain the above copyright notice and this paragraph in its entirety, (2)
 * distributions including binary code include the above copyright notice and
 * this paragraph in its entirety in the documentation or other materials
 * provided with the distribution, and (3) all advertising materials mentioning
 * features or use of this software display the following acknowledgement:
 * ``This product includes software developed by the University of California,
 * Lawrence Berkeley Laboratory and its contributors.'' Neither the name of
 * the University nor the names of its contributors may be used to endorse
 * or promote products derived from this software without specific prior
 * written permission.
 * THIS SOFTWARE IS PROVIDED ``AS IS'' AND WITHOUT ANY EXPRESS OR IMPLIED
 * WARRANTIES, INCLUDING, WITHOUT LIMITATION, THE IMPLIED WARRANTIES OF
 * MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE.
 */
#ifndef lint
static const char copyright[] =
    "@(#) Copyright (c) 1992, 1993, 1994, 1995, 1996, 1997, 1998, 1999, 2000, 2001, 2002, 2003, 2005, 2006, 2007, 2008, 2009\n\
The Regents of the University of California.  All rights reserved.\n";
static const char rcsid[] =
    "@(#) Id: nslint.c 247 2009-10-14 17:54:05Z leres  (LBL)";
#endif
/*
 * nslint - perform consistency checks on dns files
 */

#include <sys/types.h>
#include <sys/stat.h>
#include <sys/socket.h>

#include <netinet/in.h>

#include <arpa/inet.h>

#include <ctype.h>
#include <errno.h>
#ifdef HAVE_FCNTL_H
#include <fcntl.h>
#endif
#ifdef HAVE_MEMORY_H
#include <memory.h>
#endif
#include <netdb.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <unistd.h>

#include "savestr.h"
#include "version.h"

#include "gnuc.h"
#ifdef HAVE_OS_PROTO_H
#include "os-proto.h"
#endif

#define NSLINTBOOT "nslint.boot"	/* default nslint.boot file */
#define NSLINTCONF "nslint.conf"	/* default nslint.conf file */

/* Is the string just a dot by itself? */
#define CHECKDOT(p) (p[0] == '.' && p[1] == '\0')

/* Address (network order) */
struct addr {
	u_int family;
	union {
		struct in_addr _a_addr4;
		struct in6_addr _a_addr6;
	} addr;
};
#define a_addr4 addr._a_addr4.s_addr
#define a_addr6 addr._a_addr6.s6_addr

/* Network */
struct network {
	u_int family;
	union {
		struct in_addr _n_addr4;
		struct in6_addr _n_addr6;
	} addr;
	union {
		struct in_addr _n_mask4;
		struct in6_addr _n_mask6;
	} mask;
};
#define n_addr4 addr._n_addr4.s_addr
#define n_mask4 mask._n_mask4.s_addr
#define n_addr6 addr._n_addr6.s6_addr
#define n_mask6 mask._n_mask6.s6_addr

/* Item struct */
struct item {
	char *host;		/* pointer to hostname */
	struct addr addr;	/* ip address */
	u_int ttl;		/* ttl of A records */
	int records;		/* resource records seen */
	int flags;		/* flags word */
};

/* Ignored zone struct */
struct ignoredzone {
	char *zone;		/* zone name */
	int len;		/* length of zone */
};

/* Resource records seen */
#define REC_A		0x0001
#define REC_AAAA	0x0002
#define REC_PTR		0x0004
#define REC_WKS		0x0008
#define REC_HINFO	0x0010
#define REC_MX		0x0020
#define REC_CNAME	0x0040
#define REC_NS		0x0080
#define REC_SOA		0x0100
#define REC_RP		0x0200
#define REC_TXT		0x0400
#define REC_SRV		0x0800

/* These aren't real records */
#define REC_OTHER	0x1000
#define REC_REF		0x2000
#define REC_UNKNOWN	0x4000

/* resource record types for parsing */
enum rrtype {
	RR_UNDEF = 0,
	RR_A,
	RR_AAAA,
	RR_ALLOWDUPA,
	RR_CNAME,
	RR_DNSKEY,
	RR_HINFO,
	RR_MX,
	RR_NS,
	RR_PTR,
	RR_RP,
	RR_SOA,
	RR_SRV,
	RR_TXT,
	RR_WKS,
	RR_RRSIG,
	RR_NSEC,
};

/* Test for records we want to map to REC_OTHER */
#define MASK_TEST_REC (REC_WKS | REC_HINFO | \
    REC_MX | REC_SOA | REC_RP | REC_TXT | REC_SRV | REC_UNKNOWN)

/* Mask away records we don't care about in the final processing to REC_OTHER */
#define MASK_CHECK_REC \
    (REC_A | REC_AAAA | REC_PTR | REC_CNAME | REC_REF | REC_OTHER)

/* Test for records we want to check for duplicate name detection */
#define MASK_TEST_DUP \
    (REC_A | REC_AAAA | REC_HINFO | REC_CNAME)

/* Flags */
#define FLG_SELFMX	0x001	/* mx record refers to self */
#define FLG_MXREF	0x002	/* this record referred to by a mx record */
#define FLG_SMTPWKS	0x004	/* saw wks with smtp/tcp */
#define FLG_ALLOWDUPA	0x008	/* allow duplicate a records */

/* doconf() and doboot() flags */
#define CONF_MUSTEXIST	0x001	/* fatal for files to not exist */
#define CONF_NOZONE	0x002	/* do not parse zone files */

/* Test for smtp problems */
#define MASK_TEST_SMTP \
    (FLG_SELFMX | FLG_SMTPWKS)

#define ITEMSIZE (1 << 17)	/* power of two */

struct	item items[ITEMSIZE];
int	itemcnt;		/* count of items */

/* Hostname string storage */
#define STRSIZE 8192;		/* size to malloc when more space is needed */
char	*strptr;		/* pointer to string pool */
int	strsize;		/* size of space left in pool */

int	debug;
int	errors;
#ifdef __FreeBSD__
char	*bootfile = "/etc/namedb/named.boot";
char	*conffile = "/etc/namedb/named.conf";
#else
char	*bootfile = "/etc/named.boot";
char	*conffile = "/etc/named.conf";
#endif
char	*nslintboot;
char	*nslintconf;
char	*prog;
char	*cwd = ".";

static struct network *netlist;
static u_int netlistsize;	/* size of array */
static u_int netlistcnt;	/* next free element */

char **protoserv;		/* valid protocol/service names */
int protoserv_init;
int protoserv_last;
int protoserv_len;

static char inaddr[] = ".in-addr.arpa.";
static char inaddr6[] = ".ip6.arpa.";

/* XXX should be dynamic */
static struct ignoredzone ignoredzones[10];
static int numignoredzones = 0;
#define SIZEIGNOREDZONES (sizeof(ignoredzones) / sizeof(ignoredzones[0]))

/* SOA record */
#define SOA_SERIAL	0
#define SOA_REFRESH	1
#define SOA_RETRY	2
#define SOA_EXPIRE	3
#define SOA_MINIMUM	4

static u_int soaval[5];
static int nsoaval;
#define NSOAVAL (sizeof(soaval) / sizeof(soaval[0]))

/* Forwards */
void add_domain(char *, const char *);
const char *addr2str(struct addr *);
int checkaddr(const char *);
int checkdots(const char *);
void checkdups(struct item *, int);
int checkignoredzone(const char *);
int checkserv(const char *, char **p);
int checkwks(FILE *, char *, int *, char **);
int cmpaddr(const void *, const void *);
int cmpitemaddr(const void *, const void *);
int cmpitemhost(const void *, const void *);
int cmpnetwork(const void *, const void *);
void doboot(const char *, int);
void doconf(const char *, int);
const char *extractaddr(const char *, struct addr *);
const char *extractnetwork(const char *, struct network *);
struct network *findnetwork(struct addr *);
void initprotoserv(void);
int main(int, char **);
int maskwidth(struct network *);
const char *network2str(struct network *);
void nslint(void);
const char *parsenetwork(const char *);
const char *parseptr(const char *, struct addr *);
char *parsequoted(char *);
int parserrsig(const char *, char **);
int parsesoa(const char *, char **);
void process(const char *, const char *, const char *);
int rfc1034host(const char *, int);
enum rrtype txt2rrtype(const char *);
int samesubnet(struct addr *, struct addr *, struct network *);
void setmaskwidth(u_int w, struct network *);
int updateitem(const char *, struct addr *, int, u_int, int);
void usage(void) __attribute__((noreturn));

extern	char *optarg;
extern	int optind, opterr;

int
main(int argc, char **argv)
{
	char *cp;
	int op, donamedboot, donamedconf;

	if ((cp = strrchr(argv[0], '/')) != NULL)
		prog = cp + 1;
	else
		prog = argv[0];

	donamedboot = 0;
	donamedconf = 0;
	while ((op = getopt(argc, argv, "b:c:B:C:d")) != -1)
		switch (op) {

		case 'b':
			bootfile = optarg;
			++donamedboot;
			break;

		case 'c':
			conffile = optarg;
			++donamedconf;
			break;

		case 'B':
			nslintboot = optarg;
			++donamedboot;
			break;

		case 'C':
			nslintconf = optarg;
			++donamedconf;
			break;

		case 'd':
			++debug;
			break;

		default:
			usage();
		}
	if (optind != argc || (donamedboot && donamedconf))
		usage();

	/* Find config file if not manually specified */
	if (!donamedboot && !donamedconf) {
		if (access(conffile, R_OK) >= 0)
			++donamedconf;
		if (access(bootfile, R_OK) >= 0)
			++donamedboot;

		if (donamedboot && donamedconf) {
			fprintf(stderr,
			    "%s: nslint: both %s and %s exist; use -b or -c\n",
			    prog, conffile, bootfile);
			exit(1);
		}
	}

	if (donamedboot) {
		doboot(bootfile, CONF_MUSTEXIST | CONF_NOZONE);
		if (nslintboot != NULL)
			doboot(nslintboot, CONF_MUSTEXIST);
		else
			doboot(NSLINTBOOT, 0);
		doboot(bootfile, CONF_MUSTEXIST);
	} else {
		doconf(conffile, CONF_MUSTEXIST | CONF_NOZONE);
		if (nslintconf != NULL)
			doconf(nslintconf, CONF_MUSTEXIST);
		else
			doconf(NSLINTCONF, 0);
		doconf(conffile, CONF_MUSTEXIST);
	}

	/* Sort network list */
	if (netlistcnt > 0)
		qsort(netlist, netlistcnt, sizeof(netlist[0]), cmpnetwork);

	nslint();
	exit (errors != 0);
}

/* add domain if necessary */
void
add_domain(char *name, const char *domain)
{
	char *cp;

	/* Kill trailing white space and convert to lowercase */
	for (cp = name; *cp != '\0' && !isspace(*cp); ++cp)
		if (isupper(*cp))
			*cp = tolower(*cp);
	*cp-- = '\0';
	/* If necessary, append domain */
	if (cp >= name && *cp++ != '.') {
		if (*domain != '.')
			*cp++ = '.';
		(void)strcpy(cp, domain);
	}
	/* XXX should we insure a trailing dot? */
}

const char *
addr2str(struct addr *ap)
{
	struct network net;

	memset(&net, 0, sizeof(net));
	net.family = ap->family;
	switch (ap->family) {

	case AF_INET:
		net.n_addr4 = ap->a_addr4;
		setmaskwidth(32, &net);
		break;

	case AF_INET6:
		memmove(net.n_addr6, &ap->a_addr6, sizeof(ap->a_addr6));
		setmaskwidth(128, &net);
		break;

	default:
		return ("<nil>");
	}
	return (network2str(&net));
}

/*
 * Returns true if name is really an ip address.
 */
int
checkaddr(const char *name)
{
	struct in_addr addr;

	return (inet_pton(AF_INET, name, (char *)&addr));
}

/*
 * Returns true if name contains a dot but not a trailing dot.
 * Special case: allow a single dot if the second part is not one
 * of the 3 or 4 letter top level domains or is any 2 letter TLD
 */
int
checkdots(const char *name)
{
	const char *cp, *cp2;

	if ((cp = strchr(name, '.')) == NULL)
		return (0);
	cp2 = name + strlen(name) - 1;
	if (cp2 >= name && *cp2 == '.')
		return (0);

	/* Return true of more than one dot*/
	++cp;
	if (strchr(cp, '.') != NULL)
		return (1);

	if (strlen(cp) == 2 ||
	    strcasecmp(cp, "gov") == 0 ||
	    strcasecmp(cp, "edu") == 0 ||
	    strcasecmp(cp, "com") == 0 ||
	    strcasecmp(cp, "net") == 0 ||
	    strcasecmp(cp, "org") == 0 ||
	    strcasecmp(cp, "mil") == 0 ||
	    strcasecmp(cp, "int") == 0 ||
	    strcasecmp(cp, "nato") == 0 ||
	    strcasecmp(cp, "arpa") == 0)
		return (1);
	return (0);
}

/* Records we use to detect duplicates */
static struct duprec {
	int record;
	char *name;
} duprec[] = {
	{ REC_A, "a" },
	{ REC_AAAA, "aaaa" },
	{ REC_HINFO, "hinfo" },
	{ REC_CNAME, "cname" },
	{ 0, NULL },
};

void
checkdups(struct item *ip, int records)
{
	struct duprec *dp;

	records &= (ip->records & MASK_TEST_DUP);
	if (records == 0)
		return;
	for (dp = duprec; dp->name != NULL; ++dp)
		if ((records & dp->record) != 0) {
			++errors;
			fprintf(stderr, "%s: multiple \"%s\" records for %s\n",
			    prog, dp->name, ip->host);
			records &= ~dp->record;
		}
	if (records != 0)
		fprintf(stderr, "%s: checkdups: records not zero %s (0x%x)\n",
		    prog, ip->host, records);
}

/* Check for an "ignored zone" (usually dynamic dns) */
int
checkignoredzone(const char *name)
{
	int i, len, len2;

	len = strlen(name);
	if (len > 1 && name[len - 1] == '.')
		--len;
	for (i = 0; i < numignoredzones; ++i) {
		len2 = len - ignoredzones[i].len;
		if (len2 >= 0 &&
		    strncasecmp(name + len2,
			ignoredzones[i].zone, len - len2) == 0)
			    return (1);
	}
	return (0);
}

int
checkserv(const char *serv, char **p)
{
	for (; *p != NULL; ++p)
		if (*serv == **p && strcmp(serv, *p) == 0)
			return (1);
	return (0);
}

int
checkwks(FILE *f, char *proto, int *smtpp, char **errstrp)
{
	int n, sawparen;
	char *cp, *serv, **p;
	static char errstr[132];
	char buf[1024];
	char psbuf[512];

	if (!protoserv_init) {
		initprotoserv();
		++protoserv_init;
	}

	/* Line count */
	n = 0;

	/* Terminate protocol */
	cp = proto;
	while (!isspace(*cp) && *cp != '\0')
		++cp;
	if (*cp != '\0')
		*cp++ = '\0';

	/* Find services */
	*smtpp = 0;
	sawparen = 0;
	if (*cp == '(') {
		++sawparen;
		++cp;
		while (isspace(*cp))
			++cp;
	}
	for (;;) {
		if (*cp == '\0') {
			if (!sawparen)
				break;
			if (fgets(buf, sizeof(buf), f) == NULL) {
				*errstrp = "mismatched parens";
				return (n);
			}
			++n;
			cp = buf;
			while (isspace(*cp))
				++cp;
		}
		/* Find end of service, converting to lowercase */
		for (serv = cp; !isspace(*cp) && *cp != '\0'; ++cp)
			if (isupper(*cp))
				*cp = tolower(*cp);
		if (*cp != '\0')
			*cp++ = '\0';
		if (sawparen && *cp == ')') {
			/* XXX should check for trailing junk */
			break;
		}

		(void)sprintf(psbuf, "%s/%s", serv, proto);

		if (*serv == 's' && strcmp(psbuf, "tcp/smtp") == 0)
			++*smtpp;

		for (p = protoserv; *p != NULL; ++p)
			if (*psbuf == **p && strcmp(psbuf, *p) == 0) {
				break;
			}
		if (*p == NULL) {
			sprintf(errstr, "%s unknown", psbuf);
			*errstrp = errstr;
			break;
		}
	}

	return (n);
}

int
cmpaddr(const void *arg1, const void *arg2)
{
	int i, r1;
	const struct network *n1, *n2;

	n1 = (const struct network *)arg1;
	n2 = (const struct network *)arg2;

	/* IPv4 before IPv6 */
	if (n1->family != n2->family)
		return ((n1->family == AF_INET) ? -1 : 1);

	switch (n1->family) {

	case AF_INET:
		/* Address */
		if (ntohl(n1->n_addr4) < ntohl(n2->n_addr4))
			return (-1);
		else if (ntohl(n1->n_addr4) > ntohl(n2->n_addr4))
			return (1);
		return (0);

	case AF_INET6:
		/* Address */
		r1 = 0;
		for (i = 0; i < 16; ++i) {
			if (ntohl(n1->n_addr6[i]) < ntohl(n2->n_addr6[i]))
				return (-1);
			if (ntohl(n1->n_addr6[i]) > ntohl(n2->n_addr6[i]))
				return (1);
		}
		return (0);

	default:
		abort();
	}
}

int
cmpitemaddr(const void *arg1, const void *arg2)
{
	struct item *i1, *i2;

	i1 = (struct item *)arg1;
	i2 = (struct item *)arg2;

	return (cmpaddr(&i1->addr, &i2->addr));
}

int
cmpitemhost(const void *arg1, const void *arg2)
{
	struct item *i1, *i2;

	i1 = (struct item *)arg1;
	i2 = (struct item *)arg2;

	return (strcasecmp(i1->host, i1->host));
}

/* Sort by network number (use mask when networks are the same) */
int
cmpnetwork(const void *arg1, const void *arg2)
{
	int i, r1, r2;
	const struct network *n1, *n2;

	n1 = (const struct network *)arg1;
	n2 = (const struct network *)arg2;

	/* IPv4 before IPv6 */
	if (n1->family != n2->family)
		return ((n1->family == AF_INET) ? -1 : 1);

	switch (n1->family) {

	case AF_INET:
		/* Address */
		if (ntohl(n1->n_addr4) < ntohl(n2->n_addr4))
			return (-1);
		else if (ntohl(n1->n_addr4) > ntohl(n2->n_addr4))
			return (1);

		/* Mask */
		if (ntohl(n1->n_mask4) < ntohl(n2->n_mask4))
			return (1);
		else if (ntohl(n1->n_mask4) > ntohl(n2->n_mask4))
			return (-1);
		return (0);

	case AF_INET6:
		/* Address */
		r1 = 0;
		for (i = 0; i < 16; ++i) {
			if (ntohl(n1->n_addr6[i]) < ntohl(n2->n_addr6[i]))
				return (-1);
			if (ntohl(n1->n_addr6[i]) > ntohl(n2->n_addr6[i]))
				return (1);
		}

		/* Mask */
		r2 = 0;
		for (i = 0; i < 16; ++i) {
			if (n1->n_mask6[i] < n2->n_mask6[i])
				return (1);
			if (n1->n_mask6[i] > n2->n_mask6[i])
				return (-1);
		}
		return (0);
		break;

	default:
		abort();
	}
	abort();
}

void
doboot(const char *file, int flags)
{
	int n;
	char *cp, *cp2;
	FILE *f;
	const char *errstr;
	char buf[1024], name[128];

	errno = 0;
	f = fopen(file, "r");
	if (f == NULL) {
		/* Not an error if it doesn't exist */
		if ((flags & CONF_MUSTEXIST) == 0 && errno == ENOENT) {
			if (debug > 1)
				printf(
				    "%s: doit: %s doesn't exist (ignoring)\n",
				    prog, file);
			return;
		}
		fprintf(stderr, "%s: %s: %s\n", prog, file, strerror(errno));
		exit(1);
	}
	if (debug > 1)
		printf("%s: doit: opened %s\n", prog, file);

	n = 0;
	while (fgets(buf, sizeof(buf), f) != NULL) {
		++n;

		/* Skip comments */
		if (buf[0] == ';')
			continue;
		cp = strchr(buf, ';');
		if (cp)
			*cp = '\0';
		cp = buf + strlen(buf) - 1;
		if (cp >= buf && *cp == '\n')
			*cp = '\0';
		cp = buf;

		/* Eat leading whitespace */
		while (isspace(*cp))
			++cp;

		/* Skip blank lines */
		if (*cp == '\n' || *cp == '\0')
			continue;

		/* Get name */
		cp2 = cp;
		while (!isspace(*cp) && *cp != '\0')
			++cp;
		*cp++ = '\0';

		/* Find next keyword */
		while (isspace(*cp))
			++cp;
		if (strcasecmp(cp2, "directory") == 0) {
			/* Terminate directory */
			cp2 = cp;
			while (!isspace(*cp) && *cp != '\0')
				++cp;
			*cp = '\0';
			if (chdir(cp2) < 0) {
				++errors;
				fprintf(stderr, "%s: can't chdir %s: %s\n",
				    prog, cp2, strerror(errno));
				exit(1);
			}
			cwd = savestr(cp2);
			continue;
		}
		if (strcasecmp(cp2, "primary") == 0) {
			/* Extract domain, converting to lowercase */
			for (cp2 = name; !isspace(*cp) && *cp != '\0'; ++cp)
				if (isupper(*cp))
					*cp2++ = tolower(*cp);
				else
					*cp2++ = *cp;
			/* Insure trailing dot */
			if (cp2 > name && cp2[-1] != '.')
				*cp2++ = '.';
			*cp2 = '\0';

			/* Find file */
			while (isspace(*cp))
				++cp;

			/* Terminate directory */
			cp2 = cp;
			while (!isspace(*cp) && *cp != '\0')
				++cp;
			*cp = '\0';

			/* Process it! (zone is the same as the domain) */
			nsoaval = -1;
			memset(soaval, 0, sizeof(soaval));
			if ((flags & CONF_NOZONE) == 0)
				process(cp2, name, name);
			continue;
		}
		if (strcasecmp(cp2, "network") == 0) {
			errstr = parsenetwork(cp);
			if (errstr != NULL) {
				++errors;
				fprintf(stderr,
				    "%s: %s:%d: bad network: %s\n",
				    prog, file, n, errstr);
			}
			continue;
		}
		if (strcasecmp(cp2, "include") == 0) {
			/* Terminate include file */
			cp2 = cp;
			while (!isspace(*cp) && *cp != '\0')
				++cp;
			*cp = '\0';
			doboot(cp2, 1);
			continue;
		}
		/* Eat any other options */
	}
	(void)fclose(f);
}

void
doconf(const char *file, int flags)
{
	int n, fd, cc, i, depth;
	char *cp, *cp2, *buf;
	const char *p;
	char *name, *zonename, *filename, *typename;
	int namelen, zonenamelen, filenamelen, typenamelen;
	struct stat sbuf;
	char zone[128], includefile[256];

	errno = 0;
	fd = open(file, O_RDONLY, 0);
	if (fd < 0) {
		/* Not an error if it doesn't exist */
		if ((flags & CONF_MUSTEXIST) == 0 && errno == ENOENT) {
			if (debug > 1)
				printf(
				    "%s: doconf: %s doesn't exist (ignoring)\n",
				    prog, file);
			return;
		}
		fprintf(stderr, "%s: %s: %s\n", prog, file, strerror(errno));
		exit(1);
	}
	if (debug > 1)
		printf("%s: doconf: opened %s\n", prog, file);

	if (fstat(fd, &sbuf) < 0) {
		fprintf(stderr, "%s: fstat(%s) %s\n",
		    prog, file, strerror(errno));
		exit(1);
	}
	buf = (char *)malloc(sbuf.st_size + 1);
	if (buf == NULL) {
		fprintf(stderr, "%s: malloc: %s\n", prog, strerror(errno));
		exit(1);
	}

	/* Slurp entire config file */
	n = sbuf.st_size;
	cp = buf;
	do {
		cc = read(fd, cp, n);
		if (cc < 0) {
			fprintf(stderr, "%s: read(%s) %s\n",
			    prog, file, strerror(errno));
			exit(1);
		}
		cp += cc;
		n -= cc;
	} while (cc != 0 && cc < n);
	buf[cc] = '\0';

#define EATWHITESPACE \
	while (isspace(*cp)) { \
		if (*cp == '\n') \
			++n; \
		++cp; \
	}

/* Handle both to-end-of-line and C style comments */
#define EATCOMMENTS \
	{ \
	int sawcomment; \
	do { \
		EATWHITESPACE \
		sawcomment = 0; \
		if (*cp == '#') { \
			sawcomment = 1; \
			++cp; \
			while (*cp != '\n' && *cp != '\0') \
				++cp; \
		} \
		else if (strncmp(cp, "//", 2) == 0) { \
			sawcomment = 1; \
			cp += 2; \
			while (*cp != '\n' && *cp != '\0') \
				++cp; \
		} \
		else if (strncmp(cp, "/*", 2) == 0) { \
			sawcomment = 1; \
			for (cp += 2; *cp != '\0'; ++cp) { \
				if (*cp == '\n') \
					++n; \
				else if (strncmp(cp, "*/", 2) == 0) { \
					cp += 2; \
					break; \
				} \
			} \
		} \
	} while (sawcomment); \
	}

#define GETNAME(name, len) \
	{ \
	(name) = cp; \
	(len) = 0; \
	while (!isspace(*cp) && *cp != ';' && *cp != '\0') { \
		++(len); \
		++cp; \
	} \
	}

#define GETQUOTEDNAME(name, len) \
	{ \
	if (*cp != '"') { \
		++errors; \
		fprintf(stderr, "%s: %s:%d missing left quote\n", \
		    prog, file, n); \
	} else \
		++cp; \
	(name) = cp; \
	(len) = 0; \
	while (*cp != '"' && *cp != '\n' && *cp != '\0') { \
		++(len); \
		++cp; \
	} \
	if (*cp != '"') { \
		++errors; \
		fprintf(stderr, "%s: %s:%d missing right quote\n", \
		    prog, file, n); \
	} else \
		++cp; \
	}

/* Eat everything to the next semicolon, perhaps eating matching qbraces */
#define EATSEMICOLON \
	{ \
	int depth = 0; \
	while (*cp != '\0') { \
		EATCOMMENTS \
		if (*cp == ';') { \
			++cp; \
			if (depth == 0) \
				break; \
			continue; \
		} \
		if (*cp == '{') { \
			++depth; \
			++cp; \
			continue; \
		} \
		if (*cp == '}') { \
			--depth; \
			++cp; \
			continue; \
		} \
		++cp; \
	} \
	}

/* Eat everything to the next left qbrace */
#define EATSLEFTBRACE \
	while (*cp != '\0') { \
		EATCOMMENTS \
		if (*cp == '{') { \
			++cp; \
			break; \
		} \
		++cp; \
	}

	n = 1;
	zone[0] = '\0';
	cp = buf;
	while (*cp != '\0') {
		EATCOMMENTS
		if (*cp == '\0')
			break;
		GETNAME(name, namelen)
		if (namelen == 0) {
			++errors;
			fprintf(stderr, "%s: %s:%d garbage char '%c' (1)\n",
			    prog, file, n, *cp);
			++cp;
			continue;
		}
		EATCOMMENTS
		if (strncasecmp(name, "options", namelen) == 0) {
			EATCOMMENTS
			if (*cp != '{')  {
				++errors;
				fprintf(stderr,
			    "%s: %s:%d missing left qbrace in options\n",
				    prog, file, n);
			} else
				++cp;
			EATCOMMENTS
			while (*cp != '}' && *cp != '\0') {
				EATCOMMENTS
				GETNAME(name, namelen)
				if (namelen == 0) {
					++errors;
					fprintf(stderr,
					    "%s: %s:%d garbage char '%c' (2)\n",
					    prog, file, n, *cp);
					++cp;
					break;
				}

				/* If not the "directory" option, just eat it */
				if (strncasecmp(name, "directory",
				    namelen) == 0) {
					EATCOMMENTS
					GETQUOTEDNAME(cp2, i)
					cp2[i] = '\0';
					if (chdir(cp2) < 0) {
						++errors;
						fprintf(stderr,
					    "%s: %s:.%d can't chdir %s: %s\n",
						    prog, file, n, cp2,
						    strerror(errno));
						exit(1);
					}
					cwd = savestr(cp2);
				}
				EATSEMICOLON
				EATCOMMENTS
			}
			++cp;
			EATCOMMENTS
			if (*cp != ';')  {
				++errors;
				fprintf(stderr,
				    "%s: %s:%d missing options semi\n",
				    prog, file, n);
			} else
				++cp;
			continue;
		}
		if (strncasecmp(name, "zone", namelen) == 0) {
			EATCOMMENTS
			GETQUOTEDNAME(zonename, zonenamelen)
			typename = NULL;
			filename = NULL;
			typenamelen = 0;
			filenamelen = 0;
			EATCOMMENTS
			if (strncasecmp(cp, "in", 2) == 0) {
				cp += 2;
				EATWHITESPACE
			} else if (strncasecmp(cp, "chaos", 5) == 0) {
				cp += 5;
				EATWHITESPACE
			}
			if (*cp != '{')  {	/* } */
				++errors;
				fprintf(stderr,
			    "%s: %s:%d missing left qbrace in zone\n",
				    prog, file, n);
				continue;
			}
			depth = 0;
			EATCOMMENTS
			while (*cp != '\0') {
				if (*cp == '{') {
					++cp;
					++depth;
				} else if (*cp == '}') {
					if (--depth <= 1)
						break;
					++cp;
				}
				EATCOMMENTS
				GETNAME(name, namelen)
				if (namelen == 0) {
					++errors;
					fprintf(stderr,
					    "%s: %s:%d garbage char '%c' (3)\n",
					    prog, file, n, *cp);
					++cp;
					break;
				}
				if (strncasecmp(name, "type",
				    namelen) == 0) {
					EATCOMMENTS
					GETNAME(typename, typenamelen)
					if (namelen == 0) {
						++errors;
						fprintf(stderr,
					    "%s: %s:%d garbage char '%c' (4)\n",
						    prog, file, n, *cp);
						++cp;
						break;
					}
				} else if (strncasecmp(name, "file",
				    namelen) == 0) {
					EATCOMMENTS
					GETQUOTEDNAME(filename, filenamelen)
				}
				/* Just ignore keywords we don't understand */
				EATSEMICOLON
				EATCOMMENTS
			}
			/* { */
			if (*cp != '}')  {
				++errors;
				fprintf(stderr,
				    "%s: %s:%d missing zone right qbrace\n",
				    prog, file, n);
			} else
				++cp;
			if (*cp != ';')  {
				++errors;
				fprintf(stderr,
				    "%s: %s:%d missing zone semi\n",
				    prog, file, n);
			} else
				++cp;
			EATCOMMENTS
			/* If we got something interesting, process it */
			if (typenamelen == 0) {
				++errors;
				fprintf(stderr, "%s: missing zone type!\n",
				    prog);
				continue;
			}
			if (strncasecmp(typename, "master", typenamelen) == 0) {
				if (filenamelen == 0) {
					++errors;
					fprintf(stderr,
					    "%s: missing zone filename!\n",
					    prog);
					continue;
				}
				strncpy(zone, zonename, zonenamelen);
				zone[zonenamelen] = '\0';
				for (cp2 = zone; *cp2 != '\0'; ++cp2)
					if (isupper(*cp2))
						*cp2 = tolower(*cp2);
				/* Insure trailing dot */
				if (cp2 > zone && cp2[-1] != '.') {
					*cp2++ = '.';
					*cp2 = '\0';
				}
				filename[filenamelen] = '\0';
				nsoaval = -1;
				memset(soaval, 0, sizeof(soaval));
				if ((flags & CONF_NOZONE) == 0)
					process(filename, zone, zone);
			}
			continue;
		}
		if (strncasecmp(name, "nslint", namelen) == 0) {
			EATCOMMENTS
			if (*cp != '{')  {
				++errors;
				fprintf(stderr,
			    "%s: %s:%d missing left qbrace in nslint\n",
				    prog, file, n);
			} else
				++cp;
			++cp;
			EATCOMMENTS
			while (*cp != '}' && *cp != '\0') {
				EATCOMMENTS
				GETNAME(name, namelen)
				if (strncasecmp(name, "network",
				    namelen) == 0) {
					EATCOMMENTS
					GETQUOTEDNAME(cp2, i)

					cp2[i] = '\0';
					p = parsenetwork(cp2);
					if (p != NULL) {
						++errors;
						fprintf(stderr,
					    "%s: %s:%d: bad network: %s\n",
						    prog, file, n, p);
					}
				} else if (strncasecmp(name, "ignorezone",
				    namelen) == 0) {
					EATCOMMENTS
					GETQUOTEDNAME(cp2, i)
					cp2[i] = '\0';
					if (numignoredzones + 1 <
					    sizeof(ignoredzones) /
					    sizeof(ignoredzones[0])) {
						ignoredzones[numignoredzones].zone =
						    savestr(cp2);
						if (ignoredzones[numignoredzones].zone != NULL) {
							ignoredzones[numignoredzones].len = strlen(cp2);
							++numignoredzones;
						}
					}
				} else {
					++errors;
					fprintf(stderr,
					    "%s: unknown nslint \"%.*s\"\n",
					    prog, namelen, name);
				}
				EATSEMICOLON
				EATCOMMENTS
			}
			++cp;
			EATCOMMENTS
			if (*cp != ';')  {
				++errors;
				fprintf(stderr,
				    "%s: %s:%d: missing nslint semi\n",
				    prog, file, n);
			} else
				++cp;
			continue;
		}
		if (strncasecmp(name, "include", namelen) == 0) {
			EATCOMMENTS
			GETQUOTEDNAME(filename, filenamelen)
			strncpy(includefile, filename, filenamelen);
			includefile[filenamelen] = '\0';
			doconf(includefile, 1);
			EATSEMICOLON
			continue;
		}
		if (strncasecmp(name, "view", namelen) == 0) {
			EATSLEFTBRACE
			continue;
		}

		/* Skip over statements we don't understand */
		EATSEMICOLON
	}

	free(buf);
	close(fd);
}

const char *
extractaddr(const char *str, struct addr *ap)
{

	memset(ap, 0, sizeof(*ap));

	/* Let's see what we've got here */
	if (strchr(str, '.') != NULL) {
		ap->family = AF_INET;
	} else if (strchr(str, ':') != NULL) {
		ap->family = AF_INET6;
	} else
		return ("unrecognized address type");

	switch (ap->family) {

	case AF_INET:
		if (!inet_pton(ap->family, str, &ap->a_addr4))
			return ("cannot parse IPv4 address");

		break;

	case AF_INET6:
		if (!inet_pton(ap->family, str, &ap->a_addr6))
			return ("cannot parse IPv6 address");
		break;

	default:
		abort();
	}

	return (NULL);
}

const char *
extractnetwork(const char *str, struct network *np)
{
	int i;
	long w;
	char *cp, *ep;
	const char *p;
	char temp[64];

	memset(np, 0, sizeof(*np));

	/* Let's see what we've got here */
	if (strchr(str, '.') != NULL) {
		np->family = AF_INET;
		w = 32;
	} else if (strchr(str, ':') != NULL) {
		np->family = AF_INET6;
		w = 128;
	} else
		return ("unrecognized address type");

	p = strchr(str, '/');
	if (p != NULL) {
		/* Mask length was specified */
		strncpy(temp, str, sizeof(temp));
		temp[sizeof(temp) - 1] = '\0';
		cp = strchr(temp, '/');
		if (cp == NULL)
			abort();
		*cp++ = '\0';
		ep = NULL;
		w = strtol(cp, &ep, 10);
		if (*ep != '\0')
			return ("garbage following mask width");
		str = temp;
	}

	switch (np->family) {

	case AF_INET:
		if (!inet_pton(np->family, str, &np->n_addr4))
			return ("cannot parse IPv4 address");

		if (w > 32)
			return ("mask length must be <= 32");
		setmaskwidth(w, np);

		if ((np->n_addr4 & ~np->n_mask4) != 0)
			return ("non-network bits set in addr");

#ifdef notdef
		if ((ntohl(np->n_addr4) & 0xff000000) == 0)
			return ("high octet must be non-zero");
#endif
		break;

	case AF_INET6:
		if (!inet_pton(np->family, str, &np->n_addr6))
			return ("cannot parse IPv6 address");
		if (w > 128)
			return ("mask length must be <= 128");
		setmaskwidth(w, np);

		for (i = 0; i < 16; ++i) {
			if ((np->n_addr6[i] & ~np->n_mask6[i]) != 0)
				return ("non-network bits set in addr");
		}
		break;

	default:
		abort();
	}

	return (NULL);
}

struct network *
findnetwork(struct addr *ap)
{
	int i, j;
	struct network *np;

	switch (ap->family) {

	case AF_INET:
		for (i = 0, np = netlist; i < netlistcnt; ++i, ++np)
			if ((ap->a_addr4 & np->n_mask4) == np->n_addr4)
				return (np);
		break;

	case AF_INET6:
		for (i = 0, np = netlist; i < netlistcnt; ++i, ++np) {
			for (j = 0; j < sizeof(ap->a_addr6); ++j) {
				if ((ap->a_addr6[j] & np->n_mask6[j]) !=
				    np->n_addr6[j])
					break;
			}
			if (j >= sizeof(ap->a_addr6))
				return (np);
		}
		break;

	default:
		abort();
	}
	return (NULL);
}

void
initprotoserv(void)
{
	char *cp;
	struct servent *sp;
	char psbuf[512];

	protoserv_len = 256;
	protoserv = (char **)malloc(protoserv_len * sizeof(*protoserv));
	if (protoserv == NULL) {
		fprintf(stderr, "%s: nslint: malloc: %s\n",
		    prog, strerror(errno));
		exit(1);
	}

	while ((sp = getservent()) != NULL) {
		(void)sprintf(psbuf, "%s/%s", sp->s_name, sp->s_proto);

		/* Convert to lowercase */
		for (cp = psbuf; *cp != '\0'; ++cp)
			if (isupper(*cp))
				*cp = tolower(*cp);

		if (protoserv_last + 1 >= protoserv_len) {
			protoserv_len <<= 1;
			protoserv = realloc(protoserv,
			    protoserv_len * sizeof(*protoserv));
			if (protoserv == NULL) {
				fprintf(stderr, "%s: nslint: realloc: %s\n",
				    prog, strerror(errno));
				exit(1);
			}
		}
		protoserv[protoserv_last] = savestr(psbuf);
		++protoserv_last;
	}
	protoserv[protoserv_last] = NULL;
}

int
maskwidth(struct network *np)
{
	int w;
	int i, j;
	u_int32_t m, tm;

	/* Work backwards until we find a set bit */
	switch (np->family) {

	case AF_INET:
		m = ntohl(np->n_mask4);
		for (w = 32; w > 0; --w) {
			tm = 0xffffffff << (32 - w);
			if (tm == m)
				break;
		}
		break;

	case AF_INET6:
		w = 128;
		for (j = 15; j >= 0; --j) {
			m = np->n_mask6[j];
			for (i = 8; i > 0; --w, --i) {
				tm = (0xff << (8 - i)) & 0xff;
				if (tm == m)
					return (w);
			}
		}
		break;

	default:
		abort();
	}
	return (w);
}

const char *
network2str(struct network *np)
{
	int w;
	size_t len, size;
	char *cp;
	static char buf[128];

	w = maskwidth(np);
	switch (np->family) {

	case AF_INET:
		if (inet_ntop(np->family, &np->n_addr4,
		    buf, sizeof(buf)) == NULL) {
			fprintf(stderr, "network2str: v4 botch");
			abort();
		}
		if (w == 32)
			return (buf);
		break;

	case AF_INET6:
		if (inet_ntop(np->family, &np->n_addr6,
		    buf, sizeof(buf)) == NULL) {
			fprintf(stderr, "network2str: v6 botch");
			abort();
		}
		if (w == 128)
			return (buf);
		break;

	default:
		return ("<nil>");
	}

	/* Append address mask width */
	cp = buf;
	len = strlen(cp);
	cp += len;
	size = sizeof(buf) - len;
	(void)snprintf(cp, size, "/%d", w);
	return (buf);
}

void
nslint(void)
{
	int n, records, flags;
	struct item *ip, *lastaip, **ipp, **itemlist;
	struct addr addr, lastaddr;
	struct network *np;

	itemlist = (struct item **)calloc(itemcnt, sizeof(*ipp));
	if (itemlist == NULL) {
		fprintf(stderr, "%s: nslint: calloc: %s\n",
		    prog, strerror(errno));
		exit(1);
	}
	ipp = itemlist;
	for (n = 0, ip = items; n < ITEMSIZE; ++n, ++ip) {
		if (ip->host == NULL)
			continue;
		/* Save entries with addresses for later check */
		if (ip->addr.family != 0)
			*ipp++ = ip;

		if (debug > 1) {
			if (debug > 2)
				printf("%d\t", n);
			printf("%s\t%s\t0x%x\t0x%x\n",
			    ip->host, addr2str(&ip->addr),
			    ip->records, ip->flags);
		}

		/* Check for illegal hostnames (rfc1034) */
		if (rfc1034host(ip->host, ip->records))
			++errors;

		/* Check for missing ptr records (ok if also an ns record) */
		records = ip->records & MASK_CHECK_REC;
		if ((ip->records & MASK_TEST_REC) != 0)
			records |= REC_OTHER;
		switch (records) {

		case REC_A | REC_OTHER | REC_PTR | REC_REF:
		case REC_A | REC_OTHER | REC_PTR:
		case REC_A | REC_PTR | REC_REF:
		case REC_A | REC_PTR:
		case REC_AAAA | REC_OTHER | REC_PTR | REC_REF:
		case REC_AAAA | REC_OTHER | REC_PTR:
		case REC_AAAA | REC_PTR | REC_REF:
		case REC_AAAA | REC_PTR:
		case REC_CNAME:
			/* These are O.K. */
			break;

		case REC_CNAME | REC_REF:
			++errors;
			fprintf(stderr, "%s: \"cname\" referenced by other"
			    " \"cname\" or \"mx\": %s\n", prog, ip->host);
			break;

		case REC_OTHER | REC_REF:
		case REC_OTHER:
			/*
			 * This is only an error if there is an address
			 * associated with the hostname; this means
			 * there was a wks entry with bogus address.
			 * Otherwise, we have an mx or hinfo.
			 *
			 * XXX ignore localhost for now
			 * (use flag to indicate loopback?)
			 */
			if (ip->addr.family == AF_INET &&
			    ip->addr.a_addr4 != htonl(INADDR_LOOPBACK)) {
				++errors;
				fprintf(stderr,
			    "%s: \"wks\" without \"a\" and \"ptr\": %s -> %s\n",
				    prog, ip->host, addr2str(&ip->addr));
			}
			break;

		case REC_REF:
			if (!checkignoredzone(ip->host)) {
				++errors;
				fprintf(stderr, "%s: Name referenced without"
				    " other records: %s\n", prog, ip->host);
			}
			break;

		case REC_A | REC_OTHER | REC_REF:
		case REC_A | REC_OTHER:
		case REC_A | REC_REF:
		case REC_A:
		case REC_AAAA | REC_OTHER | REC_REF:
		case REC_AAAA | REC_OTHER:
		case REC_AAAA | REC_REF:
		case REC_AAAA:
			++errors;
			fprintf(stderr, "%s: Missing \"ptr\": %s -> %s\n",
			    prog, ip->host, addr2str(&ip->addr));
			break;

		case REC_OTHER | REC_PTR | REC_REF:
		case REC_OTHER | REC_PTR:
		case REC_PTR | REC_REF:
		case REC_PTR:
			++errors;
			fprintf(stderr, "%s: Missing \"a\": %s -> %s\n",
			    prog, ip->host, addr2str(&ip->addr));
			break;

		case REC_A | REC_CNAME | REC_OTHER | REC_PTR | REC_REF:
		case REC_A | REC_CNAME | REC_OTHER | REC_PTR:
		case REC_A | REC_CNAME | REC_OTHER | REC_REF:
		case REC_A | REC_CNAME | REC_OTHER:
		case REC_A | REC_CNAME | REC_PTR | REC_REF:
		case REC_A | REC_CNAME | REC_PTR:
		case REC_A | REC_CNAME | REC_REF:
		case REC_A | REC_CNAME:
		case REC_AAAA | REC_CNAME | REC_OTHER | REC_PTR | REC_REF:
		case REC_AAAA | REC_CNAME | REC_OTHER | REC_PTR:
		case REC_AAAA | REC_CNAME | REC_OTHER | REC_REF:
		case REC_AAAA | REC_CNAME | REC_OTHER:
		case REC_AAAA | REC_CNAME | REC_PTR | REC_REF:
		case REC_AAAA | REC_CNAME | REC_PTR:
		case REC_AAAA | REC_CNAME | REC_REF:
		case REC_AAAA | REC_CNAME:
		case REC_CNAME | REC_OTHER | REC_PTR | REC_REF:
		case REC_CNAME | REC_OTHER | REC_PTR:
		case REC_CNAME | REC_OTHER | REC_REF:
		case REC_CNAME | REC_OTHER:
		case REC_CNAME | REC_PTR | REC_REF:
		case REC_CNAME | REC_PTR:
			++errors;
			fprintf(stderr, "%s: \"cname\" %s has other records\n",
			    prog, ip->host);
			break;

		case 0:
			/* Second level test */
			if ((ip->records & ~(REC_NS | REC_TXT)) == 0)
				break;
			/* Fall through... */

		default:
			++errors;
			fprintf(stderr,
			    "%s: records == 0x%x: can't happen (%s 0x%x)\n",
			    prog, records, ip->host, ip->records);
			break;
		}

		/* Check for smtp problems */
		flags = ip->flags & MASK_TEST_SMTP;

		if ((flags & FLG_SELFMX) != 0 &&
		    (ip->records & (REC_A | REC_AAAA)) == 0) {
			++errors;
			fprintf(stderr,
			    "%s: Self \"mx\" for %s missing"
			    " \"a\" or \"aaaa\" record\n",
			    prog, ip->host);
		}

		switch (flags) {

		case 0:
		case FLG_SELFMX | FLG_SMTPWKS:
			/* These are O.K. */
			break;

		case FLG_SELFMX:
			if ((ip->records & REC_WKS) != 0) {
				++errors;
				fprintf(stderr,
				    "%s: smtp/tcp missing from \"wks\": %s\n",
				    prog, ip->host);
			}
			break;

		case FLG_SMTPWKS:
			++errors;
			fprintf(stderr,
			    "%s: Saw smtp/tcp without self \"mx\": %s\n",
			    prog, ip->host);
			break;

		default:
			++errors;
			fprintf(stderr,
			    "%s: flags == 0x%x: can't happen (%s)\n",
			    prog, flags, ip->host);
		}

		/* Check for chained MX records */
		if ((ip->flags & (FLG_SELFMX | FLG_MXREF)) == FLG_MXREF &&
		    (ip->records & REC_MX) != 0) {
			++errors;
			fprintf(stderr, "%s: \"mx\" referenced by other"
			    " \"mx\" record: %s\n", prog, ip->host);
		}
	}

	/* Check for doubly booked addresses */
	n = ipp - itemlist;
	qsort(itemlist, n, sizeof(itemlist[0]), cmpaddr);
	memset(&lastaddr, 0, sizeof(lastaddr));
	ip = NULL;
	for (ipp = itemlist; n > 0; ++ipp, --n) {
		addr = (*ipp)->addr;
		if (cmpaddr(&lastaddr, &addr) == 0 &&
		    ((*ipp)->flags & FLG_ALLOWDUPA) == 0 &&
		    (ip->flags & FLG_ALLOWDUPA) == 0) {
			++errors;
			fprintf(stderr, "%s: %s in use by %s and %s\n",
			    prog, addr2str(&addr), (*ipp)->host, ip->host);
		}
		memmove(&lastaddr, &addr, sizeof(addr));
		ip = *ipp;
	}

	/* Check for hosts with multiple addresses on the same subnet */
	n = ipp - itemlist;
	qsort(itemlist, n, sizeof(itemlist[0]), cmpitemhost);
	if (netlistcnt > 0) {
		n = ipp - itemlist;
		lastaip = NULL;
		for (ipp = itemlist; n > 0; ++ipp, --n) {
			ip = *ipp;
			if ((ip->records & (REC_A | REC_AAAA)) == 0 ||
			    (ip->flags & FLG_ALLOWDUPA) != 0)
				continue;
			if (lastaip != NULL &&
			    strcasecmp(ip->host, lastaip->host) == 0) {
				np = findnetwork(&ip->addr);
				if (np == NULL) {
					++errors;
					fprintf(stderr,
					    "%s: Can't find subnet mask"
					    " for %s (%s)\n",
					    prog, ip->host,
					    addr2str(&ip->addr));
				} else if (samesubnet(&lastaip->addr,
				    &ip->addr, np)) {
					++errors;
					fprintf(stderr,
			    "%s: Multiple \"a\" records for %s on subnet %s",
					    prog, ip->host,
					    network2str(np));
					fprintf(stderr, "\n\t(%s",
					    addr2str(&lastaip->addr));
					fprintf(stderr, " and %s)\n",
					    addr2str(&ip->addr));
				}
			}
			lastaip = ip;
		}
	}

	if (debug)
		printf("%s: %d/%d items used, %d error%s\n", prog, itemcnt,
		    ITEMSIZE, errors, errors == 1 ? "" : "s");
}

const char *
parsenetwork(const char *cp)
{
	const char *p;
	struct network net;

	while (isspace(*cp))
		++cp;

	p = extractnetwork(cp, &net);
	if (p != NULL)
		return (p);

	while (isspace(*cp))
		++cp;

	/* Make sure there's room */
	if (netlistsize <= netlistcnt) {
		if (netlistsize == 0) {
			netlistsize = 32;
			netlist = (struct network *)
			    malloc(netlistsize * sizeof(*netlist));
		} else {
			netlistsize <<= 1;
			netlist = (struct network *)
			    realloc(netlist, netlistsize * sizeof(*netlist));
		}
		if (netlist == NULL) {
			fprintf(stderr,
			    "%s: parsenetwork: malloc/realloc: %s\n",
			    prog, strerror(errno));
			exit(1);
		}
	}

	/* Add to list */
	memmove(netlist + netlistcnt, &net, sizeof(net));
	++netlistcnt;

	return (NULL);
}

const char *
parseptr(const char *str, struct addr *ap)
{
	int i, n, base;
	u_long v, v2;
	char *cp;
	const char *p;
	u_char *up;

	memset(ap, 0, sizeof(*ap));
	base = -1;

	/* IPv4 */
	p = str + strlen(str) - sizeof(inaddr) + 1;
	if (p >= str && strcasecmp(p, inaddr) == 0) {
		ap->family = AF_INET;
		n = 4;
		base = 10;
	} else {
		/* IPv6 */
		p = str + strlen(str) - sizeof(inaddr6) + 1;
		if (p >= str && strcasecmp(p, inaddr6) == 0) {
			ap->family = AF_INET6;
			n = 16;
			base = 16;
		}
	}

	if (base < 0)
		return ("Not a IPv4 or IPv6 \"ptr\" record");

	up = (u_char *)&ap->addr;
	for (i = 0; i < n; ++i) {
		/* Back up to previous dot or beginning of string */
		while (p > str && p[-1] != '.')
			--p;
		v = strtoul(p, &cp, base);

		if (base == 10) {
			if (v > 0xff)
				return ("Octet larger than 8 bits");
		} else {
			if (v > 0xf)
				return ("Octet larger than 4 bits");
			if (*cp != '.')
				return ("Junk in \"ptr\" record");

			/* Back up over dot */
			if (p > str)
				--p;

			/* Back up to previous dot or beginning of string */
			while (p > str && p[-1] != '.')
				--p;
			v2 = strtoul(p, &cp, base);
			if (v2 > 0xf)
				return ("Octet larger than 4 bits");
			if (*cp != '.')
				return ("Junk in \"ptr\" record");
			v = (v << 4) | v2;
		}
		if (*cp != '.')
			return ("Junk in \"ptr\" record");

		*up++ = v & 0xff;

		/* Back up over dot */
		if (p > str)
			--p;
		else if (p == str)
			break;
	}
	if (i < n - 1)
		return ("Too many octets in \"ptr\" record");
	if (p != str)
		return ("Not enough octets in \"ptr\" record");

	return (NULL);
}

/* Returns a pointer after the next token or quoted string, else NULL */
char *
parsequoted(char *cp)
{

	if (*cp == '"') {
		++cp;
		while (*cp != '"' && *cp != '\0')
			++cp;
		if (*cp != '"')
			return (NULL);
		++cp;
	} else {
		while (!isspace(*cp) && *cp != '\0')
			++cp;
	}
	return (cp);
}

/* Return true when done */
int
parserrsig(const char *str, char **errstrp)
{
	const char *cp;

	/* XXX just look for closing paren */
	cp = str + strlen(str) - 1;
	while (cp >= str)
		if (*cp-- == ')')
		return (1);
	return (0);
}

/* Return true when done */
int
parsesoa(const char *cp, char **errstrp)
{
	char ch, *garbage;
	static char errstr[132];

	/* Eat leading whitespace */
	while (isspace(*cp))
		++cp;

	/* Find opening paren */
	if (nsoaval < 0) {
		cp = strchr(cp, '(');
		if (cp == NULL)
			return (0);
		++cp;
		while (isspace(*cp))
			++cp;
		nsoaval = 0;
	}

	/* Grab any numbers we find */
	garbage = "leading garbage";
	while (isdigit(*cp) && nsoaval < NSOAVAL) {
		soaval[nsoaval] = atoi(cp);
		do {
			++cp;
		} while (isdigit(*cp));
		if (nsoaval == SOA_SERIAL && *cp == '.' && isdigit(cp[1])) {
			do {
				++cp;
			} while (isdigit(*cp));
		} else {
			ch = *cp;
			if (isupper(ch))
				ch = tolower(ch);
			switch (ch) {

			case 'w':
				soaval[nsoaval] *= 7;
				/* fall through */

			case 'd':
				soaval[nsoaval] *= 24;
				/* fall through */

			case 'h':
				soaval[nsoaval] *= 60;
				/* fall through */

			case 'm':
				soaval[nsoaval] *= 60;
				/* fall through */

			case 's':
				++cp;
				break;

			default:
				;	/* none */
			}
		}
		while (isspace(*cp))
			++cp;
		garbage = "trailing garbage";
		++nsoaval;
	}

	/* If we're done, do some sanity checks */
	if (nsoaval >= NSOAVAL && *cp == ')') {
		++cp;
		if (*cp != '\0')
			*errstrp = garbage;
		else if (soaval[SOA_EXPIRE] <
		    soaval[SOA_REFRESH] + 10 * soaval[SOA_RETRY]) {
			(void)sprintf(errstr,
		    "expire less than refresh + 10 * retry (%u < %u + 10 * %u)",
			    soaval[SOA_EXPIRE],
			    soaval[SOA_REFRESH],
			    soaval[SOA_RETRY]);
			*errstrp = errstr;
		} else if (soaval[SOA_REFRESH] < 2 * soaval[SOA_RETRY]) {
			(void)sprintf(errstr,
			    "refresh less than 2 * retry (%u < 2 * %u)",
			    soaval[SOA_REFRESH],
			    soaval[SOA_RETRY]);
			*errstrp = errstr;
		}
		return (1);
	}

	if (*cp != '\0') {
		*errstrp = garbage;
		return (1);
	}

	return (0);
}

void
process(const char *file, const char *domain, const char *zone)
{
	FILE *f;
	char ch, *cp, *cp2, *cp3, *rtype;
	const char *p;
	int n, sawsoa, sawrrsig, flags, i;
	u_int ttl;
	enum rrtype rrtype;
	struct addr *ap;
	struct addr addr;
	// struct network *net;
	int smtp;
	char buf[2048], name[256], lastname[256], odomain[256];
	char *errstr;
	const char *addrfmt =
	    "%s: %s/%s:%d \"%s\" target is an ip address: %s\n";
	const char *dotfmt =
	    "%s: %s/%s:%d \"%s\" target missing trailing dot: %s\n";

	/* Check for an "ignored zone" (usually dynamic dns) */
	if (checkignoredzone(zone))
		return;

	f = fopen(file, "r");
	if (f == NULL) {
		fprintf(stderr, "%s: %s/%s: %s\n",
		    prog, cwd, file, strerror(errno));
		++errors;
		return;
	}
	if (debug > 1)
		printf("%s: process: opened %s/%s\n", prog, cwd, file);

	/* Line number */
	n = 0;

	ap = &addr;

	lastname[0] = '\0';
	sawsoa = 0;
	sawrrsig = 0;
	while (fgets(buf, sizeof(buf), f) != NULL) {
		++n;
		cp = buf;
		while (*cp != '\0') {
			/* Handle quoted strings (but don't report errors) */
			if (*cp == '"') {
				++cp;
				while (*cp != '"' && *cp != '\n' && *cp != '\0')
					++cp;
				continue;
			}
			if (*cp == '\n' || *cp == ';')
				break;
			++cp;
		}
		*cp-- = '\0';

		/* Nuke trailing white space */
		while (cp >= buf && isspace(*cp))
			*cp-- = '\0';

		cp = buf;
		if (*cp == '\0')
			continue;

		/* Handle multi-line soa records */
		if (sawsoa) {
			errstr = NULL;
			if (parsesoa(cp, &errstr))
				sawsoa = 0;
			if (errstr != NULL) {
				++errors;
				fprintf(stderr,
				    "%s: %s/%s:%d Bad \"soa\" record (%s)\n",
				    prog, cwd, file, n, errstr);
			}
			continue;
		}

		/* Handle multi-line rrsig records */
		if (sawrrsig) {
			errstr = NULL;
			if (parserrsig(cp, &errstr))
				sawsoa = 0;
			if (errstr != NULL) {
				++errors;
				fprintf(stderr,
				    "%s: %s/%s:%d Bad \"rrsig\" record (%s)\n",
				    prog, cwd, file, n, errstr);
			}
			continue;
		}

		if (debug > 3)
			printf(">%s<\n", cp);

		/* Look for name */
		if (isspace(*cp)) {
			/* Same name as last record */
			if (lastname[0] == '\0') {
				++errors;
				fprintf(stderr,
				    "%s: %s/%s:%d No default name\n",
				    prog, cwd, file, n);
				continue;
			}
			(void)strcpy(name, lastname);
		} else {
			/* Extract name, converting to lowercase */
			for (cp2 = name; !isspace(*cp) && *cp != '\0'; ++cp)
				if (isupper(*cp))
					*cp2++ = tolower(*cp);
				else
					*cp2++ = *cp;
			*cp2 = '\0';

			/* Check for domain shorthand */
			if (name[0] == '@' && name[1] == '\0')
				(void)strcpy(name, domain);
		}

		/* Find next token */
		while (isspace(*cp))
			++cp;

		/* Handle includes (gag) */
		if (name[0] == '$' && strcasecmp(name, "$include") == 0) {
			/* Extract filename */
			cp2 = name;
			while (!isspace(*cp) && *cp != '\0')
				*cp2++ = *cp++;
			*cp2 = '\0';

			/* Look for optional domain */
			while (isspace(*cp))
				++cp;
			if (*cp == '\0')
				process(name, domain, zone);
			else {
				cp2 = cp;
				/* Convert optional domain to lowercase */
				for (; !isspace(*cp) && *cp != '\0'; ++cp)
					if (isupper(*cp))
						*cp = tolower(*cp);
				*cp = '\0';
				process(name, cp2, cp2);
			}
			continue;
		}

		/* Handle $origin */
		if (name[0] == '$' && strcasecmp(name, "$origin") == 0) {
			/* Extract domain, converting to lowercase */
			for (cp2 = odomain; !isspace(*cp) && *cp != '\0'; ++cp)
				if (isupper(*cp))
					*cp2++ = tolower(*cp);
				else
					*cp2++ = *cp;
			*cp2 = '\0';
			domain = odomain;
			lastname[0] = '\0';
			continue;
		}

		/* Handle ttl */
		if (name[0] == '$' && strcasecmp(name, "$ttl") == 0) {
			cp2 = cp;
			while (isdigit(*cp))
				++cp;
			ch = *cp;
			if (isupper(ch))
				ch = tolower(ch);
			if (strchr("wdhms", ch) != NULL)
				++cp;
			while (isspace(*cp))
				++cp;
			if (*cp != '\0') {
				++errors;
				fprintf(stderr,
				    "%s: %s/%s:%d Bad $ttl \"%s\"\n",
				    prog, cwd, file, n, cp2);
			}
			(void)strcpy(name, lastname);
			continue;
		}

		/* Parse ttl or use default  */
		if (isdigit(*cp)) {
			ttl = atoi(cp);
			do {
				++cp;
			} while (isdigit(*cp));

			ch = *cp;
			if (isupper(ch))
				ch = tolower(ch);
			switch (ch) {

			case 'w':
				ttl *= 7;
				/* fall through */

			case 'd':
				ttl *= 24;
				/* fall through */

			case 'h':
				ttl *= 60;
				/* fall through */

			case 'm':
				ttl *= 60;
				/* fall through */

			case 's':
				++cp;
				break;

			default:
				;	/* none */
			}

			if (!isspace(*cp)) {
				++errors;
				fprintf(stderr, "%s: %s/%s:%d Bad ttl\n",
				    prog, cwd, file, n);
				continue;
			}

			/* Find next token */
			++cp;
			while (isspace(*cp))
				++cp;
		} else
			ttl = soaval[SOA_MINIMUM];

		/* Eat optional "in" */
		if ((cp[0] == 'i' || cp[0] == 'I') &&
		    (cp[1] == 'n' || cp[1] == 'N') && isspace(cp[2])) {
			/* Find next token */
			cp += 3;
			while (isspace(*cp))
				++cp;
		} else if ((cp[0] == 'c' || cp[0] == 'C') &&
		    isspace(cp[5]) && strncasecmp(cp, "chaos", 5) == 0) {
			/* Find next token */
			cp += 5;
			while (isspace(*cp))
				++cp;
		}

		/* Find end of record type, converting to lowercase */
		rtype = cp;
		for (rtype = cp; !isspace(*cp) && *cp != '\0'; ++cp)
			if (isupper(*cp))
				*cp = tolower(*cp);
		*cp++ = '\0';

		/* Find "the rest" */
		while (isspace(*cp))
			++cp;

		/* Check for non-ptr names with dots but no trailing dot */
		if (!isdigit(*name) &&
		    checkdots(name) && strcmp(domain, ".") != 0) {
			++errors;
			fprintf(stderr,
			  "%s: %s/%s:%d \"%s\" name missing trailing dot: %s\n",
			    prog, cwd, file, n, rtype, name);
		}

		/* Check for FQDNs outside the zone */
		cp2 = name + strlen(name) - 1;
		if (cp2 >= name && *cp2 == '.' && strchr(name, '.') != NULL) {
			cp2 = name + strlen(name) - strlen(zone);
			if (cp2 >= name && strcasecmp(cp2, zone) != 0) {
				++errors;
				fprintf(stderr,
				    "%s: %s/%s:%d \"%s\" outside zone %s\n",
				    prog, cwd, file, n, name, zone);
			}
		}

		rrtype = txt2rrtype(rtype);
		switch (rrtype) {

		case RR_A:
			/* Handle "a" record */
			add_domain(name, domain);
			p = extractaddr(cp, ap);
			if (p != NULL) {
				++errors;
				cp2 = cp + strlen(cp) - 1;
				if (cp2 >= cp && *cp2 == '\n')
					*cp2 = '\0';
				fprintf(stderr,
			    "%s: %s/%s:%d Bad \"a\" record ip addr \"%s\"\n",
				    prog, cwd, file, n, cp);
				continue;
			}
			if (ap->family != AF_INET) {
				++errors;
				cp2 = cp + strlen(cp) - 1;
				if (cp2 >= cp && *cp2 == '\n')
					*cp2 = '\0';
				fprintf(stderr,
			    "%s: %s/%s:%d \"a\"record not AF_INET \"%s\"\n",
				    prog, cwd, file, n, cp);
				continue;
			}
			errors += updateitem(name, ap, REC_A, ttl, 0);
			break;

		case RR_AAAA:
			/* Handle "aaaa" record */
			add_domain(name, domain);
			p = extractaddr(cp, ap);
			if (p != NULL) {
				++errors;
				cp2 = cp + strlen(cp) - 1;
				if (cp2 >= cp && *cp2 == '\n')
					*cp2 = '\0';
				fprintf(stderr,
			    "%s: %s/%s:%d Bad \"aaaa\" record ip addr \"%s\"\n",
				    prog, cwd, file, n, cp);
				continue;
			}
			if (ap->family != AF_INET6) {
				++errors;
				cp2 = cp + strlen(cp) - 1;
				if (cp2 >= cp && *cp2 == '\n')
					*cp2 = '\0';
				fprintf(stderr,
			    "%s: %s/%s:%d \"aaaa\"record not AF_INET6 \"%s\"\n",
				    prog, cwd, file, n, cp);
				continue;
			}
			errors += updateitem(name, ap, REC_AAAA, ttl, 0);
			break;

		case RR_PTR:
			/* Handle "ptr" record */
			add_domain(name, domain);
			if (strcmp(cp, "@") == 0)
				(void)strcpy(cp, zone);
			if (checkdots(cp)) {
				++errors;
				fprintf(stderr,
				    checkaddr(cp) ? addrfmt : dotfmt,
				    prog, cwd, file, n, rtype, cp);
			}
			add_domain(cp, domain);
			p = parseptr(name, ap);
			if (p != NULL) {
				++errors;
				fprintf(stderr,
			"%s: %s/%s:%d Bad \"ptr\" record (%s) ip addr \"%s\"\n",
				    prog, cwd, file, n, p, name);
				continue;
			}
			errors += updateitem(cp, ap, REC_PTR, 0, 0);
			break;

		case RR_SOA:
			/* Handle "soa" record */
			if (!CHECKDOT(name)) {
				add_domain(name, domain);
				errors += updateitem(name, NULL, REC_SOA, 0, 0);
			}
			errstr = NULL;
			if (!parsesoa(cp, &errstr))
				++sawsoa;
			if (errstr != NULL) {
				++errors;
				fprintf(stderr,
				    "%s: %s/%s:%d Bad \"soa\" record (%s)\n",
				    prog, cwd, file, n, errstr);
				continue;
			}
			break;

		case RR_WKS:
			/* Handle "wks" record */
			p = extractaddr(cp, ap);
			if (p != NULL) {
				++errors;
				cp2 = cp;
				while (!isspace(*cp2) && *cp2 != '\0')
					++cp2;
				*cp2 = '\0';
				fprintf(stderr,
			    "%s: %s/%s:%d Bad \"wks\" record ip addr \"%s\"\n",
				    prog, cwd, file, n, cp);
				continue;
			}
			/* Step over ip address */
			while (*cp == '.' || isdigit(*cp))
				++cp;
			while (isspace(*cp))
				*cp++ = '\0';
			/* Make sure services are legit */
			errstr = NULL;
			n += checkwks(f, cp, &smtp, &errstr);
			if (errstr != NULL) {
				++errors;
				fprintf(stderr,
				    "%s: %s/%s:%d Bad \"wks\" record (%s)\n",
				    prog, cwd, file, n, errstr);
				continue;
			}
			add_domain(name, domain);
			errors += updateitem(name, ap, REC_WKS,
			    0, smtp ? FLG_SMTPWKS : 0);
			/* XXX check to see if ip address records exists? */
			break;

		case RR_HINFO:
			/* Handle "hinfo" record */
			add_domain(name, domain);
			errors += updateitem(name, NULL, REC_HINFO, 0, 0);
			cp2 = cp;
			cp = parsequoted(cp);
			if (cp == NULL) {
				++errors;
				fprintf(stderr,
			    "%s: %s/%s:%d \"hinfo\" missing quote: %s\n",
				    prog, cwd, file, n, cp2);
				continue;
			}
			if (!isspace(*cp)) {
				++errors;
				fprintf(stderr,
			    "%s: %s/%s:%d \"hinfo\" missing white space: %s\n",
				    prog, cwd, file, n, cp2);
				continue;
			}
			++cp;
			while (isspace(*cp))
				++cp;
			if (*cp == '\0') {
				++errors;
				fprintf(stderr,
			    "%s: %s/%s:%d \"hinfo\" missing keyword: %s\n",
				    prog, cwd, file, n, cp2);
				continue;
			}
			cp = parsequoted(cp);
			if (cp == NULL) {
				++errors;
				fprintf(stderr,
			    "%s: %s/%s:%d \"hinfo\" missing quote: %s\n",
				    prog, cwd, file, n, cp2);
				continue;
			}
			if (*cp != '\0') {
				++errors;
				fprintf(stderr,
			"%s: %s/%s:%d \"hinfo\" garbage after keywords: %s\n",
				    prog, cwd, file, n, cp2);
				continue;
			}
			break;

		case RR_MX:
			/* Handle "mx" record */
			add_domain(name, domain);
			errors += updateitem(name, NULL, REC_MX, ttl, 0);

			/* Look for priority */
			if (!isdigit(*cp)) {
				++errors;
				fprintf(stderr,
				    "%s: %s/%s:%d Bad \"mx\" priority: %s\n",
				    prog, cwd, file, n, cp);
			}

			/* Skip over priority */
			++cp;
			while (isdigit(*cp))
				++cp;
			while (isspace(*cp))
				++cp;
			if (*cp == '\0') {
				++errors;
				fprintf(stderr,
				    "%s: %s/%s:%d Missing \"mx\" hostname\n",
				    prog, cwd, file, n);
			}
			if (strcmp(cp, "@") == 0)
				(void)strcpy(cp, zone);
			if (checkdots(cp)) {
				++errors;
				fprintf(stderr,
				    checkaddr(cp) ? addrfmt : dotfmt,
				    prog, cwd, file, n, rtype, cp);
			}

			/* Check to see if mx host exists */
			add_domain(cp, domain);
			flags = FLG_MXREF;
			if (*name == *cp && strcmp(name, cp) == 0)
				flags |= FLG_SELFMX;
			errors += updateitem(cp, NULL, REC_REF, 0, flags);
			break;

		case RR_CNAME:
			/* Handle "cname" record */
			add_domain(name, domain);
			errors += updateitem(name, NULL, REC_CNAME, 0, 0);
			if (checkdots(cp)) {
				++errors;
				fprintf(stderr,
				    checkaddr(cp) ? addrfmt : dotfmt,
				    prog, cwd, file, n, rtype, cp);
			}

			/* Make sure cname points somewhere */
			if (strcmp(cp, "@") == 0)
				(void)strcpy(cp, zone);
			add_domain(cp, domain);
			errors += updateitem(cp, NULL, REC_REF, 0, 0);
			break;

		case RR_SRV:
			/* Handle "srv" record */
			add_domain(name, domain);
			errors += updateitem(name, NULL, REC_SRV, 0, 0);
			cp2 = cp;

			/* Skip over three values */
			for (i = 0; i < 3; ++i) {
				if (!isdigit(*cp)) {
					++errors;
					fprintf(stderr, "%s: %s/%s:%d"
					    " Bad \"srv\" value: %s\n",
					    prog, cwd, file, n, cp);
				}

				/* Skip over value */
				++cp;
				while (isdigit(*cp))
					++cp;
				while (isspace(*cp))
					++cp;
			}

			/* Check to see if mx host exists */
			add_domain(cp, domain);
			errors += updateitem(cp, NULL, REC_REF, 0, 0);
			break;

		case RR_TXT:
			/* Handle "txt" record */
			add_domain(name, domain);
			errors += updateitem(name, NULL, REC_TXT, 0, 0);
			cp2 = cp;
			cp = parsequoted(cp);
			if (cp == NULL) {
				++errors;
				fprintf(stderr,
				    "%s: %s/%s:%d \"txt\" missing quote: %s\n",
				    prog, cwd, file, n, cp2);
				continue;
			}
			while (isspace(*cp))
				++cp;
			if (*cp != '\0') {
				++errors;
				fprintf(stderr,
			    "%s: %s/%s:%d \"txt\" garbage after text: %s\n",
				    prog, cwd, file, n, cp2);
				continue;
			}
			break;

		case RR_NS:
			/* Handle "ns" record */
			errors += updateitem(zone, NULL, REC_NS, 0, 0);
			if (strcmp(cp, "@") == 0)
				(void)strcpy(cp, zone);
			if (checkdots(cp)) {
				++errors;
				fprintf(stderr,
				    checkaddr(cp) ? addrfmt : dotfmt,
				    prog, cwd, file, n, rtype, cp);
			}
			add_domain(cp, domain);
			errors += updateitem(cp, NULL, REC_REF, 0, 0);
			break;

		case RR_RP:
			/* Handle "rp" record */
			add_domain(name, domain);
			errors += updateitem(name, NULL, REC_RP, 0, 0);
			cp2 = cp;

			/* Step over mailbox name */
			/* XXX could add_domain() and check further */
			while (!isspace(*cp) && *cp != '\0')
				++cp;
			if (*cp == '\0') {
				++errors;
				fprintf(stderr,
			    "%s: %s/%s:%d \"rp\" missing text name: %s\n",
				    prog, cwd, file, n, cp2);
				continue;
			}
			++cp;
			cp3 = cp;

			/* Step over text name */
			while (!isspace(*cp) && *cp != '\0')
				++cp;

			if (*cp != '\0') {
				++errors;
				fprintf(stderr,
			    "%s: %s/%s:%d \"rp\" garbage after text name: %s\n",
				    prog, cwd, file, n, cp2);
				continue;
			}

			/* Make sure text name points somewhere (if not ".") */
			if (!CHECKDOT(cp3)) {
				add_domain(cp3, domain);
				errors += updateitem(cp3, NULL, REC_REF, 0, 0);
			}
			break;

		case RR_ALLOWDUPA:
			/* Handle "allow duplicate a" record */
			add_domain(name, domain);
			p = extractaddr(cp, ap);
			if (p != NULL) {
				++errors;
				cp2 = cp + strlen(cp) - 1;
				if (cp2 >= cp && *cp2 == '\n')
					*cp2 = '\0';
				fprintf(stderr,
		    "%s: %s/%s:%d Bad \"allowdupa\" record ip addr \"%s\"\n",
				    prog, cwd, file, n, cp);
				continue;
			}
			errors += updateitem(name, ap, 0, 0, FLG_ALLOWDUPA);
			break;

		case RR_DNSKEY:
			/* Handle "dnskey" record */
			add_domain(name, domain);
			errors += updateitem(name, NULL, REC_CNAME, 0, 0);
			if (checkdots(cp)) {
				++errors;
				fprintf(stderr,
				    checkaddr(cp) ? addrfmt : dotfmt,
				    prog, cwd, file, n, rtype, cp);
			}

			/* Make sure cname points somewhere */
			if (strcmp(cp, "@") == 0)
				(void)strcpy(cp, zone);
			add_domain(cp, domain);
			errors += updateitem(cp, NULL, REC_REF, 0, 0);
			break;

		case RR_RRSIG:
			errstr = NULL;
			if (!parserrsig(cp, &errstr))
				++sawrrsig;
			if (errstr != NULL) {
				++errors;
				fprintf(stderr,
				    "%s: %s/%s:%d Bad \"rrsig\" record (%s)\n",
				    prog, cwd, file, n, errstr);
				continue;
			}
			break;

		case RR_NSEC:
			/* XXX */
			continue;

		default:
			/* Unknown record type */
			++errors;
			fprintf(stderr,
			    "%s: %s/%s:%d Unknown record type \"%s\"\n",
			    prog, cwd, file, n, rtype);
			add_domain(name, domain);
			errors += updateitem(name, NULL, REC_UNKNOWN, 0, 0);
			break;
		}
		(void)strcpy(lastname, name);
	}
	(void)fclose(f);
	return;
}

static const char *microlist[] = {
	"_tcp",
	"_udp",
	"_msdcs",
	"_sites",
	NULL
};

int
rfc1034host(const char *host, int recs)
{
	const char *cp, **p;
	int underok;

	underok = 0;
	for (p = microlist; *p != NULL ;++p)
		if ((cp = strstr(host, *p)) != NULL &&
		    cp > host &&
		    cp[-1] == '.' &&
		    cp[strlen(*p)] == '.') {
			++underok;
			break;
		}

	cp = host;
	if (!(isalpha(*cp) || isdigit(*cp) || (*cp == '_' && underok))) {
		fprintf(stderr,
	    "%s: illegal hostname \"%s\" (starts with non-alpha/numeric)\n",
		    prog, host);
		return (1);
	}
	for (++cp; *cp != '.' && *cp != '\0'; ++cp)
		if (!(isalpha(*cp) || isdigit(*cp) || *cp == '-' ||
		    (*cp == '/' && (recs & REC_SOA) != 0))) {
			fprintf(stderr,
		    "%s: Illegal hostname \"%s\" ('%c' illegal character)\n",
			    prog, host, *cp);
			return (1);
		}
	if (--cp >= host && *cp == '-') {
		fprintf(stderr, "%s: Illegal hostname \"%s\" (ends with '-')\n",
		    prog, host);
		return (1);
	}
	return (0);
}

enum rrtype
txt2rrtype(const char *str)
{
	if (strcasecmp(str, "aaaa") == 0)
		return (RR_AAAA);
	if (strcasecmp(str, "a") == 0)
		return (RR_A);
	if (strcasecmp(str, "allowdupa") == 0)
		return (RR_ALLOWDUPA);
	if (strcasecmp(str, "cname") == 0)
		return (RR_CNAME);
	if (strcasecmp(str, "dnskey") == 0)
		return (RR_DNSKEY);
	if (strcasecmp(str, "hinfo") == 0)
		return (RR_HINFO);
	if (strcasecmp(str, "mx") == 0)
		return (RR_MX);
	if (strcasecmp(str, "ns") == 0)
		return (RR_NS);
	if (strcasecmp(str, "ptr") == 0)
		return (RR_PTR);
	if (strcasecmp(str, "rp") == 0)
		return (RR_RP);
	if (strcasecmp(str, "soa") == 0)
		return (RR_SOA);
	if (strcasecmp(str, "srv") == 0)
		return (RR_SRV);
	if (strcasecmp(str, "txt") == 0)
		return (RR_TXT);
	if (strcasecmp(str, "wks") == 0)
		return (RR_WKS);
	if (strcasecmp(str, "RRSIG") == 0)
		return (RR_RRSIG);
	if (strcasecmp(str, "NSEC") == 0)
		return (RR_NSEC);
	return (RR_UNDEF);
}

int
samesubnet(struct addr *a1, struct addr *a2, struct network *np)
{
	int i;
	u_int32_t v1, v2;

	/* IPv4 before IPv6 */
	if (a1->family != a2->family)
		return (0);

	switch (a1->family) {

	case AF_INET:
		/* Apply the mask to both values */
		v1 = a1->a_addr4 & np->n_mask4;
		v2 = a2->a_addr4 & np->n_mask4;
		return (v1 == v2);

	case AF_INET6:
		/* Apply the mask to both values */
		for (i = 0; i < 16; ++i) {
			v1 = a1->a_addr6[i] & np->n_mask6[i];
			v2 = a2->a_addr6[i] & np->n_mask6[i];
			if (v1 != v2)
				return (0);
		}
		break;

	default:
		abort();
	}
	return (1);
}

/* Set address mask in network order */
void
setmaskwidth(u_int w, struct network *np)
{
	int i, j;

	switch (np->family) {

	case AF_INET:
		if (w <= 0)
			np->n_mask4 = 0;
		else
			np->n_mask4 = htonl(0xffffffff << (32 - w));
		break;

	case AF_INET6:
		/* XXX is this right? */
		memset(np->n_mask6, 0, sizeof(np->n_mask6));
		for (i = 0; i < w / 8; ++i)
			np->n_mask6[i] = 0xff;
		i = w / 8;
		j = w % 8;
		if (j > 0 && i < 16)
			np->n_mask6[i] = 0xff << (8 - j);
		break;

	default:
		abort();
	}
}

int
updateitem(const char *host, struct addr *ap, int records, u_int ttl, int flags)
{
	const char *ccp;
	int n, errs;
	u_int i;
	struct item *ip;
	int foundsome;

	n = 0;
	foundsome = 0;
	errs = 0;

	/* Hash the host name */
	i = 0;
	ccp = host;
	while (*ccp != '\0')
		i = i * 37 + *ccp++;
	ip = &items[i & (ITEMSIZE - 1)];

	/* Look for a match or any empty slot */
	while (n < ITEMSIZE && ip->host != NULL) {

		if ((ap == NULL || ip->addr.family == 0 ||
		    cmpaddr(ap, &ip->addr) == 0) &&
		    *host == *ip->host && strcmp(host, ip->host) == 0) {
			++foundsome;
			if (ip->addr.family == 0 && ap != NULL)
				memmove(&ip->addr, ap, sizeof(*ap));
			if ((records & MASK_TEST_DUP) != 0)
				checkdups(ip, records);
			ip->records |= records;
			/* Only check differing ttl's for A and MX records */
			if (ip->ttl == 0)
				ip->ttl = ttl;
			else if (ttl != 0 && ip->ttl != ttl) {
				fprintf(stderr,
				    "%s: Differing ttls for %s (%u != %u)\n",
				    prog, ip->host, ttl, ip->ttl);
				++errs;
			}
			ip->flags |= flags;
			/* Not done if we wildcard matched the name */
			if (ap != NULL)
				return (errs);
		}
		++n;
		++ip;
		if (ip >= &items[ITEMSIZE])
			ip = items;
	}

	if (n >= ITEMSIZE) {
		fprintf(stderr, "%s: Out of item slots (max %d)\n",
		    prog, ITEMSIZE);
		exit(1);
	}

	/* Done if we were wildcarding the name (and found entries for it) */
	if (ap == NULL && foundsome) {
		return (errs);
	}

	/* Didn't find it, make new entry */
	++itemcnt;
	if (ip->host) {
		fprintf(stderr, "%s: Reusing bucket!\n", prog);
		exit(1);
	}
	if (ap != NULL)
		memmove(&ip->addr, ap, sizeof(*ap));
	ip->host = savestr(host);
	if ((records & MASK_TEST_DUP) != 0)
		checkdups(ip, records);
	ip->records |= records;
	if (ttl != 0)
		ip->ttl = ttl;
	ip->flags |= flags;
	return (errs);
}

void
usage(void)
{

	fprintf(stderr, "Version %s\n", version);
	fprintf(stderr, "usage: %s [-d] [-b named.boot] [-B nslint.boot]\n",
	    prog);
	fprintf(stderr, "       %s [-d] [-c named.conf] [-C nslint.conf]\n",
	    prog);
	exit(1);
}
