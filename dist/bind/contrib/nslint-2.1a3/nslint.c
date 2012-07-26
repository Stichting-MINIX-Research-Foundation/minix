/*
 * Copyright (c) 1992, 1993, 1994, 1995, 1996, 1997, 1998, 1999, 2000, 2001
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
    "@(#) Copyright (c) 1992, 1993, 1994, 1995, 1996, 1997, 1998, 1999, 2000, 2001\n\
The Regents of the University of California.  All rights reserved.\n";
static const char rcsid[] =
    "@(#) $Id: nslint.c,v 1.1 2001-12-21 04:12:04 marka Exp $ (LBL)";
#endif
/*
 * nslint - perform consistency checks on dns files
 */

#include <sys/types.h>
#include <sys/stat.h>

#include <netinet/in.h>

#include <arpa/inet.h>

#include <ctype.h>
#include <errno.h>
#ifdef HAVE_FCNTL_H
#include <fcntl.h>
#endif
#ifdef HAVE_MALLOC_H
#include <malloc.h>
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

#include "gnuc.h"
#ifdef HAVE_OS_PROTO_H
#include "os-proto.h"
#endif

#define NSLINTBOOT "nslint.boot"	/* default nslint.boot file */
#define NSLINTCONF "nslint.conf"	/* default nslint.conf file */

/* item struct */
struct item {
	char *host;		/* pointer to hostname */
	u_int32_t addr;		/* ip address */
	u_int ttl;		/* ttl of A records */
	int records;		/* resource records seen */
	int flags;		/* flags word */
};

/* Resource records seen */
#define REC_A		0x0001
#define REC_PTR		0x0002
#define REC_WKS		0x0004
#define REC_HINFO	0x0008
#define REC_MX		0x0010
#define REC_CNAME	0x0020
#define REC_NS		0x0040
#define REC_SOA		0x0080
#define REC_RP		0x0100
#define REC_TXT		0x0200
#define REC_SRV		0x0400

/* These aren't real records */
#define REC_OTHER	0x0800
#define REC_REF		0x1000
#define REC_UNKNOWN	0x2000

/* Test for records we want to map to REC_OTHER */
#define MASK_TEST_REC (REC_WKS | REC_HINFO | \
    REC_MX | REC_SOA | REC_RP | REC_TXT | REC_SRV | REC_UNKNOWN)

/* Mask away records we don't care about in the final processing to REC_OTHER */
#define MASK_CHECK_REC \
    (REC_A | REC_PTR | REC_CNAME | REC_REF | REC_OTHER)

/* Test for records we want to check for duplicate name detection */
#define MASK_TEST_DUP \
    (REC_A | REC_HINFO)

/* Flags */
#define FLG_SELFMX	0x001	/* mx record refers to self */
#define FLG_MXREF	0x002	/* this record referred to by a mx record */
#define FLG_SMTPWKS	0x004	/* saw wks with smtp/tcp */
#define FLG_ALLOWDUPA	0x008	/* allow duplicate a records */

/* Test for smtp problems */
#define MASK_TEST_SMTP \
    (FLG_SELFMX | FLG_SMTPWKS)


#define ITEMSIZE (1 << 17)	/* power of two */
#define ITEMHASH(str, h, p) \
    for (p = str, h = 0; *p != '.' && *p != '\0';) h = (h << 5) - h + *p++

struct	item items[ITEMSIZE];
int	itemcnt;		/* count of items */

/* Hostname string storage */
#define STRSIZE 8192;		/* size to malloc when more space is needed */
char	*strptr;		/* pointer to string pool */
int	strsize;		/* size of space left in pool */

int	debug;
int	errors;
char	*bootfile = "/etc/named.boot";
char	*conffile = "/etc/named.conf";
char	*nslintboot;
char	*nslintconf;
char	*prog;
char	*cwd = ".";

char **protoserv;		/* valid protocol/service names */
int protoserv_init;
int protoserv_last;
int protoserv_len;

static char inaddr[] = ".in-addr.arpa.";

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
static	inline void add_domain(char *, const char *);
int	checkdots(const char *);
void	checkdups(struct item *, int);
int	checkserv(const char *, char **p);
int	checkwks(FILE *, char *, int *, char **);
int	cmpaddr(const void *, const void *);
int	cmphost(const void *, const void *);
int	doboot(const char *, int);
int	doconf(const char *, int);
void	initprotoserv(void);
char	*intoa(u_int32_t);
int	main(int, char **);
int	nslint(void);
int	parseinaddr(const char *, u_int32_t *, u_int32_t *);
int	parsenetwork(const char *, char **);
u_int32_t parseptr(const char *, u_int32_t, u_int32_t, char **);
char	*parsequoted(char *);
int	parsesoa(const char *, char **);
void	process(const char *, const char *, const char *);
int	rfc1034host(const char *, int);
int	updateitem(const char *, u_int32_t, int, u_int, int);
__dead	void usage(void) __attribute__((volatile));

extern	char *optarg;
extern	int optind, opterr;

/* add domain if necessary */
static inline void
add_domain(register char *name, register const char *domain)
{
	register char *cp;

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

int
main(int argc, char **argv)
{
	register char *cp;
	register int op, status, i, donamedboot, donamedconf;

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

	if (donamedboot)
		status = doboot(bootfile, 1);
	else if (donamedconf)
		status = doconf(conffile, 1);
	else {
		status = doconf(conffile, 0);
		if (status < 0) {
			status = doboot(bootfile, 1);
			++donamedboot;
		} else
			++donamedconf;
	}

	if (donamedboot) {
		if (nslintboot != NULL)
			status |= doboot(nslintboot, 1);
		else if ((i = doboot(NSLINTBOOT, 0)) > 0)
			status |= i;
	} else {
		if (nslintconf != NULL)
			status |= doconf(nslintconf, 1);
		else if ((i = doconf(NSLINTCONF, 0)) > 0)
			status |= i;
	}
	status |= nslint();
	exit (status);
}

struct netlist {
	u_int32_t net;
	u_int32_t mask;
};

static struct netlist *netlist;
static u_int netlistsize;	/* size of array */
static u_int netlistcnt;	/* next free element */

static u_int32_t
findmask(u_int32_t addr)
{
	register int i;

	for (i = 0; i < netlistcnt; ++i)
		if ((addr & netlist[i].mask) == netlist[i].net)
			return (netlist[i].mask);
	return (0);
}

int
parsenetwork(register const char *cp, register char **errstrp)
{
	register int i, w;
	register u_int32_t net, mask;
	register u_int32_t o;
	register int shift;
	static char errstr[132];

	while (isspace(*cp))
		++cp;
	net = 0;
	mask = 0;
	shift = 24;
	while (isdigit(*cp) && shift >= 0) {
		o = 0;
		do {
			o = o * 10 + (*cp++ - '0');
		} while (isdigit(*cp));
		net |= o << shift;
		shift -= 8;
		if (*cp != '.')
			break;
		++cp;
	}


	if (isspace(*cp)) {
		++cp;
		while (isspace(*cp))
			++cp;
		mask = htonl(inet_addr(cp));
		if ((int)mask == -1) {
			*errstrp = errstr;
			(void)sprintf(errstr, "bad mask \"%s\"", cp);
			return (0);
		}
		i = 0;
		while (isdigit(*cp))
			++cp;
		for (i = 0; i < 3 && *cp == '.'; ++i) {
			++cp;
			while (isdigit(*cp))
				++cp;
		}
		if (i != 3) {
			*errstrp = "wrong number of dots in mask";
			return (0);
		}
	} else if (*cp == '/') {
		++cp;
		w = atoi(cp);
		do {
			++cp;
		} while (isdigit(*cp));
		if (w < 1 || w > 32) {
			*errstrp = "bad mask width";
			return (0);
		}
		mask = 0xffffffff << (32 - w);
	} else {
		*errstrp = "garbage after net";
		return (0);
	}

	while (isspace(*cp))
		++cp;

	if (*cp != '\0') {
		*errstrp = "trailing garbage";
		return (0);
	}

	/* Finaly sanity checks */
	if ((net & ~ mask) != 0) {
		*errstrp = errstr;
		(void)sprintf(errstr, "host bits set in net \"%s\"",
		    intoa(net));
		return (0);
	}

	/* Make sure there's room */
	if (netlistsize <= netlistcnt) {
		if (netlistsize == 0) {
			netlistsize = 32;
			netlist = (struct netlist *)
			    malloc(netlistsize * sizeof(*netlist));
		} else {
			netlistsize <<= 1;
			netlist = (struct netlist *)
			    realloc(netlist, netlistsize * sizeof(*netlist));
		}
		if (netlist == NULL) {
			fprintf(stderr, "%s: nslint: malloc/realloc: %s\n",
			    prog, strerror(errno));
			exit(1);
		}
	}

	/* Add to list */
	netlist[netlistcnt].net = net;
	netlist[netlistcnt].mask = mask;
	++netlistcnt;

	return (1);
}

int
doboot(register const char *file, register int mustexist)
{
	register int n;
	register char *cp, *cp2;
	register FILE *f;
	char *errstr;
	char buf[1024], name[128];

	errno = 0;
	f = fopen(file, "r");
	if (f == NULL) {
		/* Not an error if it doesn't exist */
		if (!mustexist && errno == ENOENT) {
			if (debug > 1)
				printf(
				    "%s: doit: %s doesn't exist (ignoring)\n",
				    prog, file);
			return (-1);
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
			process(cp2, name, name);
			continue;
		}
		if (strcasecmp(cp2, "network") == 0) {
			if (!parsenetwork(cp, &errstr)) {
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
			errors += doboot(cp2, 1);
			continue;
		}
		/* Eat any other options */
	}
	(void)fclose(f);

	return (errors != 0);
}

int
doconf(register const char *file, register int mustexist)
{
	register int n, fd, cc, i, depth;
	register char *cp, *cp2, *buf;
	register char *name, *zonename, *filename, *typename;
	register int namelen, zonenamelen, filenamelen, typenamelen;
	char *errstr;
	struct stat sbuf;
	char zone[128], includefile[256];

	errno = 0;
	fd = open(file, O_RDONLY, 0);
	if (fd < 0) {
		/* Not an error if it doesn't exist */
		if (!mustexist && errno == ENOENT) {
			if (debug > 1)
				printf(
				    "%s: doconf: %s doesn't exist (ignoring)\n",
				    prog, file);
			return (-1);
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
	register int depth = 0; \
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
					if (!parsenetwork(cp2, &errstr)) {
						++errors;
						fprintf(stderr,
					    "%s: %s:%d: bad network: %s\n",
						    prog, file, n, errstr);
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
				fprintf(stderr, "missing options semi\n");
			} else
				++cp;
			continue;
		}
		if (strncasecmp(name, "include", namelen) == 0) {
			EATCOMMENTS
			GETQUOTEDNAME(filename, filenamelen)
			strncpy(includefile, filename, filenamelen);
			includefile[filenamelen] = '\0';
			errors += doconf(includefile, 1);
			EATSEMICOLON
			continue;
		}

		/* Skip over statements we don't understand */
		EATSEMICOLON
	}

	free(buf);
	close(fd);
	return (errors != 0);
}

/* Return true when done */
int
parsesoa(register const char *cp, register char **errstrp)
{
	register char ch, *garbage;
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
process(register const char *file, register const char *domain,
    register const char *zone)
{
	register FILE *f;
	register char ch, *cp, *cp2, *cp3, *rtype;
	register const char *ccp;
	register int n, sawsoa, flags, i;
	register u_int ttl;
	register u_int32_t addr;
	u_int32_t net, mask;
	int smtp;
	char buf[1024], name[128], lastname[128], odomain[128];
	char *errstr;
	char *dotfmt = "%s: %s/%s:%d \"%s\" target missing trailing dot: %s\n";

	f = fopen(file, "r");
	if (f == NULL) {
		fprintf(stderr, "%s: %s/%s: %s\n",
		    prog, cwd, file, strerror(errno));
		++errors;
		return;
	}
	if (debug > 1)
		printf("%s: process: opened %s/%s\n", prog, cwd, file);

	/* Are we doing an in-addr.arpa domain? */
	n = 0;
	net = 0;
	mask = 0;
	ccp = domain + strlen(domain) - sizeof(inaddr) + 1;
	if (ccp >= domain && strcasecmp(ccp, inaddr) == 0 &&
	    !parseinaddr(domain, &net, &mask)) {
		++errors;
		fprintf(stderr, "%s: %s/%s:%d bad in-addr.arpa domain\n",
		    prog, cwd, file, n);
		return;
	}

	lastname[0] = '\0';
	sawsoa = 0;
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
				    "%s: %s/%s:%d bad \"soa\" record (%s)\n",
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
				    "%s: %s/%s:%d no default name\n",
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

			/* Are we doing an in-addr.arpa domain? */
			net = 0;
			mask = 0;
			ccp = domain + strlen(domain) - (sizeof(inaddr) - 1);
			if (ccp >= domain && strcasecmp(ccp, inaddr) == 0 &&
			    !parseinaddr(domain, &net, &mask)) {
				++errors;
				fprintf(stderr,
				    "%s: %s/%s:%d bad in-addr.arpa domain\n",
				    prog, cwd, file, n);
				return;
			}
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
				    "%s: %s/%s:%d bad $ttl \"%s\"\n",
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
				fprintf(stderr, "%s: %s/%s:%d bad ttl\n",
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

#define CHECK4(p, a, b, c, d) \
    (p[0] == (a) && p[1] == (b) && p[2] == (c) && p[3] == (d) && p[4] == '\0')
#define CHECK3(p, a, b, c) \
    (p[0] == (a) && p[1] == (b) && p[2] == (c) && p[3] == '\0')
#define CHECK2(p, a, b) \
    (p[0] == (a) && p[1] == (b) && p[2] == '\0')
#define CHECKDOT(p) \
    (p[0] == '.' && p[1] == '\0')

		if (rtype[0] == 'a' && rtype[1] == '\0') {
			/* Handle "a" record */
			add_domain(name, domain);
			addr = htonl(inet_addr(cp));
			if ((int)addr == -1) {
				++errors;
				cp2 = cp + strlen(cp) - 1;
				if (cp2 >= cp && *cp2 == '\n')
					*cp2 = '\0';
				fprintf(stderr,
			    "%s: %s/%s:%d bad \"a\" record ip addr \"%s\"\n",
				    prog, cwd, file, n, cp);
				continue;
			}
			errors += updateitem(name, addr, REC_A, ttl, 0);
		} else if (CHECK4(rtype, 'a', 'a', 'a', 'a')) {
			/* Just eat for now */
			continue;
		} else if (CHECK3(rtype, 'p', 't', 'r')) {
			/* Handle "ptr" record */
			add_domain(name, domain);
			if (strcmp(cp, "@") == 0)
				(void)strcpy(cp, zone);
			if (checkdots(cp)) {
				++errors;
				fprintf(stderr, dotfmt,
				    prog, cwd, file, n, rtype, cp);
			}
			add_domain(cp, domain);
			errstr = NULL;
			addr = parseptr(name, net, mask, &errstr);
			if (errstr != NULL) {
				++errors;
				fprintf(stderr,
			"%s: %s/%s:%d bad \"ptr\" record (%s) ip addr \"%s\"\n",
				    prog, cwd, file, n, errstr, name);
				continue;
			}
			errors += updateitem(cp, addr, REC_PTR, 0, 0);
		} else if (CHECK3(rtype, 's', 'o', 'a')) {
			/* Handle "soa" record */
			if (!CHECKDOT(name)) {
				add_domain(name, domain);
				errors += updateitem(name, 0, REC_SOA, 0, 0);
			}
			errstr = NULL;
			if (!parsesoa(cp, &errstr))
				++sawsoa;
			if (errstr != NULL) {
				++errors;
				fprintf(stderr,
				    "%s: %s/%s:%d bad \"soa\" record (%s)\n",
				    prog, cwd, file, n, errstr);
				continue;
			}
		} else if (CHECK3(rtype, 'w', 'k', 's')) {
			/* Handle "wks" record */
			addr = htonl(inet_addr(cp));
			if ((int)addr == -1) {
				++errors;
				cp2 = cp;
				while (!isspace(*cp2) && *cp2 != '\0')
					++cp2;
				*cp2 = '\0';
				fprintf(stderr,
			    "%s: %s/%s:%d bad \"wks\" record ip addr \"%s\"\n",
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
				    "%s: %s/%s:%d bad \"wks\" record (%s)\n",
				    prog, cwd, file, n, errstr);
				continue;
			}
			add_domain(name, domain);
			errors += updateitem(name, addr, REC_WKS,
			    0, smtp ? FLG_SMTPWKS : 0);
			/* XXX check to see if ip address records exists? */
		} else if (rtype[0] == 'h' && strcmp(rtype, "hinfo") == 0) {
			/* Handle "hinfo" record */
			add_domain(name, domain);
			errors += updateitem(name, 0, REC_HINFO, 0, 0);
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
		} else if (CHECK2(rtype, 'm', 'x')) {
			/* Handle "mx" record */
			add_domain(name, domain);
			errors += updateitem(name, 0, REC_MX, ttl, 0);

			/* Look for priority */
			if (!isdigit(*cp)) {
				++errors;
				fprintf(stderr,
				    "%s: %s/%s:%d bad \"mx\" priority: %s\n",
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
				    "%s: %s/%s:%d missing \"mx\" hostname\n",
				    prog, cwd, file, n);
			}
			if (strcmp(cp, "@") == 0)
				(void)strcpy(cp, zone);
			if (checkdots(cp)) {
				++errors;
				fprintf(stderr, dotfmt,
				    prog, cwd, file, n, rtype, cp);
			}

			/* Check to see if mx host exists */
			add_domain(cp, domain);
			flags = FLG_MXREF;
			if (*name == *cp && strcmp(name, cp) == 0)
				flags |= FLG_SELFMX;
			errors += updateitem(cp, 0, REC_REF, 0, flags);
		} else if (rtype[0] == 'c' && strcmp(rtype, "cname") == 0) {
			/* Handle "cname" record */
			add_domain(name, domain);
			errors += updateitem(name, 0, REC_CNAME, 0, 0);
			if (checkdots(cp)) {
				++errors;
				fprintf(stderr, dotfmt,
				    prog, cwd, file, n, rtype, cp);
			}

			/* Make sure cname points somewhere */
			if (strcmp(cp, "@") == 0)
				(void)strcpy(cp, zone);
			add_domain(cp, domain);
			errors += updateitem(cp, 0, REC_REF, 0, 0);
		} else if (CHECK3(rtype, 's', 'r', 'v')) {
			/* Handle "srv" record */
			add_domain(name, domain);
			errors += updateitem(name, 0, REC_SRV, 0, 0);
			cp2 = cp;

			/* Skip over three values */
			for (i = 0; i < 3; ++i) {
				if (!isdigit(*cp)) {
					++errors;
					fprintf(stderr, "%s: %s/%s:%d"
					    " bad \"srv\" value: %s\n",
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
			errors += updateitem(cp, 0, REC_REF, 0, 0);
		} else if (CHECK3(rtype, 't', 'x', 't')) {
			/* Handle "txt" record */
			add_domain(name, domain);
			errors += updateitem(name, 0, REC_TXT, 0, 0);
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
		} else if (CHECK2(rtype, 'n', 's')) {
			/* Handle "ns" record */
			errors += updateitem(zone, 0, REC_NS, 0, 0);
			if (strcmp(cp, "@") == 0)
				(void)strcpy(cp, zone);
			if (checkdots(cp)) {
				++errors;
				fprintf(stderr, dotfmt,
				    prog, cwd, file, n, rtype, cp);
			}
			add_domain(cp, domain);
			errors += updateitem(cp, 0, REC_REF, 0, 0);
		} else if (CHECK2(rtype, 'r', 'p')) {
			/* Handle "rp" record */
			add_domain(name, domain);
			errors += updateitem(name, 0, REC_RP, 0, 0);
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
				errors += updateitem(cp3, 0, REC_REF, 0, 0);
			}
		} else if (rtype[0] == 'a' && strcmp(rtype, "allowdupa") == 0) {
			/* Handle "allow duplicate a" record */
			add_domain(name, domain);
			addr = htonl(inet_addr(cp));
			if ((int)addr == -1) {
				++errors;
				cp2 = cp + strlen(cp) - 1;
				if (cp2 >= cp && *cp2 == '\n')
					*cp2 = '\0';
				fprintf(stderr,
		    "%s: %s/%s:%d bad \"allowdupa\" record ip addr \"%s\"\n",
				    prog, cwd, file, n, cp);
				continue;
			}
			errors += updateitem(name, addr, 0, 0, FLG_ALLOWDUPA);
		} else {
			/* Unknown record type */
			++errors;
			fprintf(stderr,
			    "%s: %s/%s:%d unknown record type \"%s\"\n",
			    prog, cwd, file, n, rtype);
			add_domain(name, domain);
			errors += updateitem(name, 0, REC_UNKNOWN, 0, 0);
		}
		(void)strcpy(lastname, name);
	}
	(void)fclose(f);
	return;
}

/* Records we use to detect duplicates */
static struct duprec {
	int record;
	char *name;
} duprec[] = {
	{ REC_A, "a" },
	{ REC_HINFO, "hinfo" },
	{ 0, NULL },
};

void
checkdups(register struct item *ip, register int records)
{
	register struct duprec *dp;

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
		fprintf(stderr, "%s: checkdups: records not zero (%d)\n",
		    prog, records);
}

int
updateitem(register const char *host, register u_int32_t addr,
    register int records, register u_int ttl, register int flags)
{
	register const char *ccp;
	register int n, errs;
	register u_int i;
	register struct item *ip;
	int foundsome;

	n = 0;
	foundsome = 0;
	errs = 0;
	ITEMHASH(host, i, ccp);
	ip = &items[i & (ITEMSIZE - 1)];
	while (n < ITEMSIZE && ip->host) {
		if ((addr == 0 || addr == ip->addr || ip->addr == 0) &&
		    *host == *ip->host && strcmp(host, ip->host) == 0) {
			++foundsome;
			if (ip->addr == 0)
				ip->addr = addr;
			if ((records & MASK_TEST_DUP) != 0)
				checkdups(ip, records);
			ip->records |= records;
			/* Only check differing ttl's for A and MX records */
			if (ip->ttl == 0)
				ip->ttl = ttl;
			else if (ttl != 0 && ip->ttl != ttl) {
				fprintf(stderr,
				    "%s: differing ttls for %s (%u != %u)\n",
				    prog, ip->host, ttl, ip->ttl);
				++errs;
			}
			ip->flags |= flags;
			/* Not done if we wildcard matched the name */
			if (addr)
				return (errs);
		}
		++n;
		++ip;
		if (ip >= &items[ITEMSIZE])
			ip = items;
	}

	if (n >= ITEMSIZE) {
		fprintf(stderr, "%s: out of item slots (max %d)\n",
		    prog, ITEMSIZE);
		exit(1);
	}

	/* Done if we were wildcarding the name (and found entries for it) */
	if (addr == 0 && foundsome)
		return (errs);

	/* Didn't find it, make new entry */
	++itemcnt;
	if (ip->host) {
		fprintf(stderr, "%s: reusing bucket!\n", prog);
		exit(1);
	}
	ip->addr = addr;
	ip->host = savestr(host);
	if ((records & MASK_TEST_DUP) != 0)
		checkdups(ip, records);
	ip->records |= records;
	if (ttl != 0)
		ip->ttl = ttl;
	ip->flags |= flags;
	return (errs);
}

static const char *microlist[] = {
	"_tcp",
	"_udp",
	"_msdcs",
	"_sites",
	NULL
};

int
rfc1034host(register const char *host, register int recs)
{
	register const char *cp, **p;
	register int underok;

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
		    "%s: illegal hostname \"%s\" ('%c' illegal character)\n",
			    prog, host, *cp);
			return (1);
		}
	if (--cp >= host && *cp == '-') {
		fprintf(stderr, "%s: illegal hostname \"%s\" (ends with '-')\n",
		    prog, host);
		return (1);
	}
	return (0);
}

int
nslint(void)
{
	register int n, records, flags;
	register struct item *ip, *lastaip, **ipp, **itemlist;
	register u_int32_t addr, lastaddr, mask;

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
		if (ip->addr != 0)
			*ipp++ = ip;

		if (debug > 1) {
			if (debug > 2)
				printf("%d\t", n);
			printf("%s\t%s\t0x%x\t0x%x\n",
			    ip->host, intoa(ip->addr), ip->records, ip->flags);
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
			 */
			if (ip->addr != 0) {
				++errors;
				fprintf(stderr,
			    "%s: \"wks\" without \"a\" and \"ptr\": %s -> %s\n",
				    prog, ip->host, intoa(ip->addr));
			}
			break;

		case REC_REF:
			++errors;
			fprintf(stderr,
			    "%s: name referenced without other records: %s\n",
			    prog, ip->host);
			break;

		case REC_A | REC_OTHER | REC_REF:
		case REC_A | REC_OTHER:
		case REC_A | REC_REF:
		case REC_A:
			++errors;
			fprintf(stderr, "%s: missing \"ptr\": %s -> %s\n",
			    prog, ip->host, intoa(ip->addr));
			break;

		case REC_OTHER | REC_PTR | REC_REF:
		case REC_OTHER | REC_PTR:
		case REC_PTR | REC_REF:
		case REC_PTR:
			++errors;
			fprintf(stderr, "%s: missing \"a\": %s -> %s\n",
			    prog, ip->host, intoa(ip->addr));
			break;

		case REC_A | REC_CNAME | REC_OTHER | REC_PTR | REC_REF:
		case REC_A | REC_CNAME | REC_OTHER | REC_PTR:
		case REC_A | REC_CNAME | REC_OTHER | REC_REF:
		case REC_A | REC_CNAME | REC_OTHER:
		case REC_A | REC_CNAME | REC_PTR | REC_REF:
		case REC_A | REC_CNAME | REC_PTR:
		case REC_A | REC_CNAME | REC_REF:
		case REC_A | REC_CNAME:
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

		if ((flags & FLG_SELFMX) != 0 && (ip->records & REC_A) == 0) {
			++errors;
			fprintf(stderr,
			    "%s: self \"mx\" for %s missing \"a\" record\n",
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
			    "%s: saw smtp/tcp without self \"mx\": %s\n",
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
	lastaddr = 0;
	ip = NULL;
	for (ipp = itemlist; n > 0; ++ipp, --n) {
		addr = (*ipp)->addr;
		if (lastaddr == addr &&
		    ((*ipp)->flags & FLG_ALLOWDUPA) == 0 &&
		    (ip->flags & FLG_ALLOWDUPA) == 0) {
			++errors;
			fprintf(stderr, "%s: %s in use by %s and %s\n",
			    prog, intoa(addr), (*ipp)->host, ip->host);
		}
		lastaddr = addr;
		ip = *ipp;
	}

	/* Check for hosts with multiple addresses on the same subnet */
	n = ipp - itemlist;
	qsort(itemlist, n, sizeof(itemlist[0]), cmphost);
	if (netlistcnt > 0) {
		n = ipp - itemlist;
		lastaip = NULL;
		for (ipp = itemlist; n > 0; ++ipp, --n) {
			ip = *ipp;
			if ((ip->records & REC_A) == 0 ||
			    (ip->flags & FLG_ALLOWDUPA) != 0)
				continue;
			if (lastaip != NULL &&
			    strcasecmp(ip->host, lastaip->host) == 0) {
				mask = findmask(ip->addr);
				if (mask == 0) {
					++errors;
					fprintf(stderr,
					    "%s: can't find mask for %s (%s)\n",
					    prog, ip->host, intoa(ip->addr));
				} else if ((lastaip->addr & mask) ==
				    (ip->addr & mask) ) {
					++errors;
					fprintf(stderr,
			    "%s: multiple \"a\" records for %s on subnet %s",
					    prog, ip->host,
					    intoa(ip->addr & mask));
					fprintf(stderr, "\n\t(%s",
					    intoa(lastaip->addr));
					fprintf(stderr, " and %s)\n",
					    intoa(ip->addr));
				}
			}
			lastaip = ip;
		}
	}

	if (debug)
		printf("%s: %d/%d items used, %d error%s\n", prog, itemcnt,
		    ITEMSIZE, errors, errors == 1 ? "" : "s");
	return (errors != 0);
}

/* Similar to inet_ntoa() */
char *
intoa(u_int32_t addr)
{
	register char *cp;
	register u_int byte;
	register int n;
	static char buf[sizeof(".xxx.xxx.xxx.xxx")];

	cp = &buf[sizeof buf];
	*--cp = '\0';

	n = 4;
	do {
		byte = addr & 0xff;
		*--cp = byte % 10 + '0';
		byte /= 10;
		if (byte > 0) {
			*--cp = byte % 10 + '0';
			byte /= 10;
			if (byte > 0)
				*--cp = byte + '0';
		}
		*--cp = '.';
		addr >>= 8;
	} while (--n > 0);

	return cp + 1;
}

int
parseinaddr(register const char *cp, register u_int32_t *netp,
    register u_int32_t *maskp)
{
	register int i, bits;
	register u_int32_t o, net, mask;

	if (!isdigit(*cp))
		return (0);
	net = 0;
	mask = 0xff000000;
	bits = 0;
	o = 0;
	do {
		o = o * 10 + (*cp++ - '0');
	} while (isdigit(*cp));
	net = o << 24;

	/* Check for classless delegation mask width */
	if (*cp == '/') {
		++cp;
		o = 0;
		do {
			o = o * 10 + (*cp++ - '0');
		} while (isdigit(*cp));
		bits = o;
		if (bits <= 0 || bits > 32)
			return (0);
	}

	if (*cp == '.' && isdigit(cp[1])) {
		++cp;
		o = 0;
		do {
			o = o * 10 + (*cp++ - '0');
		} while (isdigit(*cp));
		net = (net >> 8) | (o << 24);
		mask = 0xffff0000;
		if (*cp == '.' && isdigit(cp[1])) {
			++cp;
			o = 0;
			do {
				o = o * 10 + (*cp++ - '0');
			} while (isdigit(*cp));
			net = (net >> 8) | (o << 24);
			mask = 0xffffff00;
			if (*cp == '.' && isdigit(cp[1])) {
				++cp;
				o = 0;
				do {
					o = o * 10 + (*cp++ - '0');
				} while (isdigit(*cp));
				net = (net >> 8) | (o << 24);
				mask = 0xffffffff;
			}
		}
	}
	if (strcasecmp(cp, inaddr) != 0)
		return (0);

	/* Classless delegation */
	/* XXX check that calculated mask isn't smaller than octet mask? */
	if (bits != 0)
		for (mask = 0, i = 31; bits > 0; --i, --bits)
			mask |= (1 << i);

	*netp = net;
	*maskp = mask;
	return (1);
}

u_int32_t
parseptr(register const char *cp, u_int32_t net, u_int32_t mask,
    register char **errstrp)
{
	register u_int32_t o, addr;
	register int shift;

	addr = 0;
	shift = 0;
	while (isdigit(*cp) && shift < 32) {
		o = 0;
		do {
			o = o * 10 + (*cp++ - '0');
		} while (isdigit(*cp));
		addr |= o << shift;
		shift += 8;
		if (*cp != '.') {
			if (*cp == '\0')
				break;
			*errstrp = "missing dot";
			return (0);
		}
		++cp;
	}

	if (shift > 32) {
		*errstrp = "more than 4 octets";
		return (0);
	}

	if (shift == 32 && strcasecmp(cp, inaddr + 1) == 0)
		return (addr);

#ifdef notdef
	if (*cp != '\0') {
		*errstrp = "trailing junk";
		return (0);
	}
#endif
#ifdef notdef
	if ((~mask & net) != 0) {
		*errstrp = "too many octets for net";
		return (0);
	}
#endif
	return (net | addr);
}

int
checkwks(register FILE *f, register char *proto, register int *smtpp,
    register char **errstrp)
{
	register int n, sawparen;
	register char *cp, *serv, **p;
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
checkserv(register const char *serv, register char **p)
{
	for (; *p != NULL; ++p)
		if (*serv == **p && strcmp(serv, *p) == 0)
			return (1);
	return (0);
}

void
initprotoserv(void)
{
	register char *cp;
	register struct servent *sp;
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

/*
 * Returns true if name contains a dot but not a trailing dot.
 * Special case: allow a single dot if the second part is not one
 * of the 3 or 4 letter top level domains or is any 2 letter TLD
 */
int
checkdots(register const char *name)
{
	register const char *cp, *cp2;

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

int
cmpaddr(register const void *ip1, register const void *ip2)
{
	register u_int32_t a1, a2;

	a1 = (*(struct item **)ip1)->addr;
	a2 = (*(struct item **)ip2)->addr;

	if (a1 < a2)
		return (-1);
	else if (a1 > a2)
		return (1);
	else
		return (0);
}

int
cmphost(register const void *ip1, register const void *ip2)
{
	register const char *s1, *s2;

	s1 = (*(struct item **)ip1)->host;
	s2 = (*(struct item **)ip2)->host;

	return (strcasecmp(s1, s2));
}

/* Returns a pointer after the next token or quoted string, else NULL */
char *
parsequoted(register char *cp)
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

__dead void
usage(void)
{
	extern char version[];

	fprintf(stderr, "Version %s\n", version);
	fprintf(stderr, "usage: %s [-d] [-b named.boot] [-B nslint.boot]\n",
	    prog);
	fprintf(stderr, "       %s [-d] [-c named.conf] [-C nslint.conf]\n",
	    prog);
	exit(1);
}
