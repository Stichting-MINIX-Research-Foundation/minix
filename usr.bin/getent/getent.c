/*	$NetBSD: getent.c,v 1.19 2012/03/15 02:02:23 joerg Exp $	*/

/*-
 * Copyright (c) 2004-2006 The NetBSD Foundation, Inc.
 * All rights reserved.
 *
 * This code is derived from software contributed to The NetBSD Foundation
 * by Luke Mewburn.
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
 * THIS SOFTWARE IS PROVIDED BY THE NETBSD FOUNDATION, INC. AND CONTRIBUTORS
 * ``AS IS'' AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED
 * TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL THE FOUNDATION OR CONTRIBUTORS
 * BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 * CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
 * POSSIBILITY OF SUCH DAMAGE.
 */

#include <sys/cdefs.h>
#ifndef lint
__RCSID("$NetBSD: getent.c,v 1.19 2012/03/15 02:02:23 joerg Exp $");
#endif /* not lint */

#include <sys/socket.h>

#include <assert.h>
#include <ctype.h>
#include <errno.h>
#include <grp.h>
#include <limits.h>
#include <netdb.h>
#include <netgroup.h>
#include <pwd.h>
#include <stdio.h>
#include <stdarg.h>
#include <stdbool.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <paths.h>
#include <err.h>

#include <arpa/inet.h>
#include <arpa/nameser.h>

#include <net/if.h>
#include <net/if_ether.h>

#include <netinet/in.h>		/* for INET6_ADDRSTRLEN */

#if !defined(__minix)
#include <rpc/rpcent.h>
#endif /* !defined(__minix) */

#include <disktab.h>

static int	usage(void) __attribute__((__noreturn__));
static int	parsenum(const char *, unsigned long *);
static int	disktab(int, char *[]);
static int	gettytab(int, char *[]);
static int	ethers(int, char *[]);
static int	group(int, char *[]);
static int	hosts(int, char *[]);
static int	netgroup(int, char *[]);
static int	networks(int, char *[]);
static int	passwd(int, char *[]);
static int	printcap(int, char *[]);
static int	protocols(int, char *[]);
#if !defined(__minix)
static int	rpc(int, char *[]);
#endif /* !defined(__minix) */
static int	services(int, char *[]);
static int	shells(int, char *[]);

enum {
	RV_OK		= 0,
	RV_USAGE	= 1,
	RV_NOTFOUND	= 2,
	RV_NOENUM	= 3
};

static struct getentdb {
	const char	*name;
	int		(*callback)(int, char *[]);
} databases[] = {
	{	"disktab",	disktab,	},
	{	"ethers",	ethers,		},
	{	"gettytab",	gettytab,	},
	{	"group",	group,		},
	{	"hosts",	hosts,		},
	{	"netgroup",	netgroup,	},
	{	"networks",	networks,	},
	{	"passwd",	passwd,		},
	{	"printcap",	printcap,	},
	{	"protocols",	protocols,	},
#if !defined(__minix)
	{	"rpc",		rpc,		},
#endif /* !defined(__minix) */
	{	"services",	services,	},
	{	"shells",	shells,		},

	{	NULL,		NULL,		},
};


int
main(int argc, char *argv[])
{
	struct getentdb	*curdb;

	setprogname(argv[0]);

	if (argc < 2)
		usage();
	for (curdb = databases; curdb->name != NULL; curdb++)
		if (strcmp(curdb->name, argv[1]) == 0)
			return (*curdb->callback)(argc, argv);

	warn("Unknown database `%s'", argv[1]);
	usage();
	/* NOTREACHED */
}

static int
usage(void)
{
	struct getentdb	*curdb;
	size_t i;

	(void)fprintf(stderr, "Usage: %s database [key ...]\n",
	    getprogname());
	(void)fprintf(stderr, "\tdatabase may be one of:");
	for (i = 0, curdb = databases; curdb->name != NULL; curdb++, i++) {
		if (i % 7 == 0)
			(void)fputs("\n\t\t", stderr);
		(void)fprintf(stderr, "%s%s", i % 7 == 0 ? "" : " ",
		    curdb->name);
	}
	(void)fprintf(stderr, "\n");
	exit(RV_USAGE);
	/* NOTREACHED */
}

static int
parsenum(const char *word, unsigned long *result)
{
	unsigned long	num;
	char		*ep;

	assert(word != NULL);
	assert(result != NULL);

	if (!isdigit((unsigned char)word[0]))
		return 0;
	errno = 0;
	num = strtoul(word, &ep, 10);
	if (num == ULONG_MAX && errno == ERANGE)
		return 0;
	if (*ep != '\0')
		return 0;
	*result = num;
	return 1;
}

/*
 * printfmtstrings --
 *	vprintf(format, ...),
 *	then the aliases (beginning with prefix, separated by sep),
 *	then a newline
 */
static __printflike(4, 5) void
printfmtstrings(char *strings[], const char *prefix, const char *sep,
    const char *fmt, ...)
{
	va_list		ap;
	const char	*curpref;
	size_t		i;

	va_start(ap, fmt);
	(void)vprintf(fmt, ap);
	va_end(ap);

	curpref = prefix;
	for (i = 0; strings[i] != NULL; i++) {
		(void)printf("%s%s", curpref, strings[i]);
		curpref = sep;
	}
	(void)printf("\n");
}


		/*
		 * ethers
		 */

static int
ethers(int argc, char *argv[])
{
	char		hostname[MAXHOSTNAMELEN + 1], *hp;
	struct ether_addr ea, *eap;
	int		i, rv;

	assert(argc > 1);
	assert(argv != NULL);

#define ETHERSPRINT	(void)printf("%-17s  %s\n", ether_ntoa(eap), hp)

	rv = RV_OK;
	if (argc == 2) {
		warnx("Enumeration not supported on ethers");
		rv = RV_NOENUM;
	} else {
		for (i = 2; i < argc; i++) {
			if ((eap = ether_aton(argv[i])) == NULL) {
				eap = &ea;
				hp = argv[i];
				if (ether_hostton(hp, eap) != 0) {
					rv = RV_NOTFOUND;
					break;
				}
			} else {
				hp = hostname;
				if (ether_ntohost(hp, eap) != 0) {
					rv = RV_NOTFOUND;
					break;
				}
			}
			ETHERSPRINT;
		}
	}
	return rv;
}

		/*
		 * group
		 */

static int
group(int argc, char *argv[])
{
	struct group	*gr;
	unsigned long	id;
	int		i, rv;

	assert(argc > 1);
	assert(argv != NULL);

#define GROUPPRINT	printfmtstrings(gr->gr_mem, ":", ",", "%s:%s:%u", \
			    gr->gr_name, gr->gr_passwd, gr->gr_gid)

	(void)setgroupent(1);
	rv = RV_OK;
	if (argc == 2) {
		while ((gr = getgrent()) != NULL)
			GROUPPRINT;
	} else {
		for (i = 2; i < argc; i++) {
			if (parsenum(argv[i], &id))
				gr = getgrgid((gid_t)id);
			else
				gr = getgrnam(argv[i]);
			if (gr != NULL)
				GROUPPRINT;
			else {
				rv = RV_NOTFOUND;
				break;
			}
		}
	}
	endgrent();
	return rv;
}


		/*
		 * hosts
		 */

static void
hostsprint(const struct hostent *he)
{
	char	buf[INET6_ADDRSTRLEN];

	assert(he != NULL);
	if (inet_ntop(he->h_addrtype, he->h_addr, buf, sizeof(buf)) == NULL)
		(void)strlcpy(buf, "# unknown", sizeof(buf));
	printfmtstrings(he->h_aliases, "  ", " ", "%-16s  %s", buf, he->h_name);
}

static int
hosts(int argc, char *argv[])
{
	struct hostent	*he;
	char		addr[IN6ADDRSZ];
	int		i, rv;

	assert(argc > 1);
	assert(argv != NULL);

	sethostent(1);
	rv = RV_OK;
	if (argc == 2) {
		while ((he = gethostent()) != NULL)
			hostsprint(he);
	} else {
		for (i = 2; i < argc; i++) {
			if (inet_pton(AF_INET6, argv[i], (void *)addr) > 0)
				he = gethostbyaddr(addr, IN6ADDRSZ, AF_INET6);
			else if (inet_pton(AF_INET, argv[i], (void *)addr) > 0)
				he = gethostbyaddr(addr, INADDRSZ, AF_INET);
			else
				he = gethostbyname(argv[i]);
			if (he != NULL)
				hostsprint(he);
			else {
				rv = RV_NOTFOUND;
				break;
			}
		}
	}
	endhostent();
	return rv;
}

		/*
		 * netgroup
		 */
static int
netgroup(int argc, char *argv[])
{
	int		rv, i;
	bool		first;
	const char	*host, *user, *domain;

	assert(argc > 1);
	assert(argv != NULL);

#define NETGROUPPRINT(s)	(((s) != NULL) ? (s) : "")

	rv = RV_OK;
	if (argc == 2) {
		warnx("Enumeration not supported on netgroup");
		rv = RV_NOENUM;
	} else {
		for (i = 2; i < argc; i++) {
			setnetgrent(argv[i]);
			first = true;
			while (getnetgrent(&host, &user, &domain) != 0) {
				if (first) {
					first = false;
					(void)fputs(argv[i], stdout);
				}
				(void)printf(" (%s,%s,%s)",
				    NETGROUPPRINT(host),
				    NETGROUPPRINT(user),
				    NETGROUPPRINT(domain));
			}
			if (!first)
				(void)putchar('\n');
			endnetgrent();
		}
	}

	return rv;
}

		/*
		 * networks
		 */

static void
networksprint(const struct netent *ne)
{
	char		buf[INET6_ADDRSTRLEN];
	struct	in_addr	ianet;

	assert(ne != NULL);
	ianet = inet_makeaddr(ne->n_net, 0);
	if (inet_ntop(ne->n_addrtype, &ianet, buf, sizeof(buf)) == NULL)
		(void)strlcpy(buf, "# unknown", sizeof(buf));
	printfmtstrings(ne->n_aliases, "  ", " ", "%-16s  %s", ne->n_name, buf);
}

static int
networks(int argc, char *argv[])
{
	struct netent	*ne;
	in_addr_t	net;
	int		i, rv;

	assert(argc > 1);
	assert(argv != NULL);

	setnetent(1);
	rv = RV_OK;
	if (argc == 2) {
		while ((ne = getnetent()) != NULL)
			networksprint(ne);
	} else {
		for (i = 2; i < argc; i++) {
			net = inet_network(argv[i]);
			if (net != INADDR_NONE)
				ne = getnetbyaddr(net, AF_INET);
			else
				ne = getnetbyname(argv[i]);
			if (ne != NULL)
				networksprint(ne);
			else {
				rv = RV_NOTFOUND;
				break;
			}
		}
	}
	endnetent();
	return rv;
}


		/*
		 * passwd
		 */

static int
passwd(int argc, char *argv[])
{
	struct passwd	*pw;
	unsigned long	id;
	int		i, rv;

	assert(argc > 1);
	assert(argv != NULL);

#define PASSWDPRINT	(void)printf("%s:%s:%u:%u:%s:%s:%s\n", \
			    pw->pw_name, pw->pw_passwd, pw->pw_uid, \
			    pw->pw_gid, pw->pw_gecos, pw->pw_dir, pw->pw_shell)

	(void)setpassent(1);
	rv = RV_OK;
	if (argc == 2) {
		while ((pw = getpwent()) != NULL)
			PASSWDPRINT;
	} else {
		for (i = 2; i < argc; i++) {
			if (parsenum(argv[i], &id))
				pw = getpwuid((uid_t)id);
			else
				pw = getpwnam(argv[i]);
			if (pw != NULL)
				PASSWDPRINT;
			else {
				rv = RV_NOTFOUND;
				break;
			}
		}
	}
	endpwent();
	return rv;
}

static char *
mygetent(const char * const * db_array, const char *name)
{
	char *buf = NULL;
	int error;

	switch (error = cgetent(&buf, db_array, name)) {
	case -3:
		warnx("tc= loop in record `%s' in `%s'", name, db_array[0]);
		break;
	case -2:
		warn("system error fetching record `%s' in `%s'", name,
		    db_array[0]);
		break;
	case -1:
	case 0:
		break;
	case 1:
		warnx("tc= reference not found in record for `%s' in `%s'",
		    name, db_array[0]);
		break;
	default:
		warnx("unknown error %d in record `%s' in `%s'", error, name,
		    db_array[0]);
		break;
	}
	return buf;
}

static char *
mygetone(const char * const * db_array, int first)
{
	char *buf = NULL;
	int error;

	switch (error = (first ? cgetfirst : cgetnext)(&buf, db_array)) {
	case -2:
		warnx("tc= loop in `%s'", db_array[0]);
		break;
	case -1:
		warn("system error fetching record in `%s'", db_array[0]);
		break;
	case 0:
	case 1:
		break;
	case 2:
		warnx("tc= reference not found in `%s'", db_array[0]);
		break;
	default:
		warnx("unknown error %d in `%s'", error, db_array[0]);
		break;
	}
	return buf;
}

static void
capprint(const char *cap)
{
	char *c = strchr(cap, ':');
	if (c)
		if (c == cap)
			(void)printf("true\n");
		else {
			int l = (int)(c - cap);
			(void)printf("%*.*s\n", l, l, cap);
		}
	else
		(void)printf("%s\n", cap);
}

static void
prettyprint(char *b)
{
#define TERMWIDTH 65
	int did = 0;
	size_t len;
	char *s, c;

	for (;;) {
		len = strlen(b);
		if (len <= TERMWIDTH) {
done:
			if (did)
				printf("\t:");
			printf("%s\n", b);
			return;
		}
		for (s = b + TERMWIDTH; s > b && *s != ':'; s--)
			continue;
		if (*s++ != ':')
			goto done;
		c = *s;
		*s = '\0';
		if (did)
			printf("\t:");
		did++;
		printf("%s\\\n", b);
		*s = c;
		b = s;
	}
}

static void
handleone(const char * const *db_array, char *b, int recurse, int pretty,
    int level)
{
	char *tc;

	if (level && pretty)
		printf("\n");
	if (pretty)
		prettyprint(b);
	else
		printf("%s\n", b);
	if (!recurse || cgetstr(b, "tc", &tc) <= 0)
		return;

	b = mygetent(db_array, tc);
	free(tc);

	if (b == NULL)
		return;

	handleone(db_array, b, recurse, pretty, ++level);
	free(b);
}

static int
handlecap(const char *db, int argc, char *argv[])
{
	static const char sfx[] = "=#:";
	const char *db_array[] = { db, NULL };
	char	*b, *cap;
	int	i, rv, c;
	size_t	j;
	int	expand = 1, recurse = 0, pretty = 0;

	assert(argc > 1);
	assert(argv != NULL);

	argc--;
	argv++;
	while ((c = getopt(argc, argv, "pnr")) != -1)
		switch (c) {
		case 'n':
			expand = 0;
			break;
		case 'r':
			expand = 0;
			recurse = 1;
			break;
		case 'p':
			pretty = 1;
			break;
		default:
			usage();
			break;
		}

	argc -= optind;
	argv += optind;
	csetexpandtc(expand);
	rv = RV_OK;
	if (argc == 0) {
		for (b = mygetone(db_array, 1); b; b = mygetone(db_array, 0)) {
			handleone(db_array, b, recurse, pretty, 0);
			free(b);
		}
	} else {
		if ((b = mygetent(db_array, argv[0])) == NULL)
			return RV_NOTFOUND;
		if (argc == 1)
			handleone(db_array, b, recurse, pretty, 0);
		else {
			for (i = 2; i < argc; i++) {
				for (j = 0; j < sizeof(sfx) - 1; j++) {
					cap = cgetcap(b, argv[i], sfx[j]);
					if (cap) {
						capprint(cap);
						break;
					} 
				}
				if (j == sizeof(sfx) - 1)
					printf("false\n");
			}
		}
		free(b);
	}
	return rv;
}

		/*
		 * gettytab
		 */

static int
gettytab(int argc, char *argv[])
{
	return handlecap(_PATH_GETTYTAB, argc, argv);
}

		/*
		 * printcap
		 */

static int
printcap(int argc, char *argv[])
{
	return handlecap(_PATH_PRINTCAP, argc, argv);
}

		/*
		 * disktab
		 */

static int
disktab(int argc, char *argv[])
{
	return handlecap(_PATH_DISKTAB, argc, argv);
}

		/*
		 * protocols
		 */

static int
protocols(int argc, char *argv[])
{
	struct protoent	*pe;
	unsigned long	id;
	int		i, rv;

	assert(argc > 1);
	assert(argv != NULL);

#define PROTOCOLSPRINT	printfmtstrings(pe->p_aliases, "  ", " ", \
			    "%-16s  %5d", pe->p_name, pe->p_proto)

	setprotoent(1);
	rv = RV_OK;
	if (argc == 2) {
		while ((pe = getprotoent()) != NULL)
			PROTOCOLSPRINT;
	} else {
		for (i = 2; i < argc; i++) {
			if (parsenum(argv[i], &id))
				pe = getprotobynumber((int)id);
			else
				pe = getprotobyname(argv[i]);
			if (pe != NULL)
				PROTOCOLSPRINT;
			else {
				rv = RV_NOTFOUND;
				break;
			}
		}
	}
	endprotoent();
	return rv;
}

#if !defined(__minix)
		/*
		 * rpc
		 */

static int
rpc(int argc, char *argv[])
{
	struct rpcent	*re;
	unsigned long	id;
	int		i, rv;
	
	assert(argc > 1);
	assert(argv != NULL);

#define RPCPRINT	printfmtstrings(re->r_aliases, "  ", " ", \
				"%-16s  %6d", \
				re->r_name, re->r_number)

	setrpcent(1);
	rv = RV_OK;
	if (argc == 2) {
		while ((re = getrpcent()) != NULL)
			RPCPRINT;
	} else {
		for (i = 2; i < argc; i++) {
			if (parsenum(argv[i], &id))
				re = getrpcbynumber((int)id);
			else
				re = getrpcbyname(argv[i]);
			if (re != NULL)
				RPCPRINT;
			else {
				rv = RV_NOTFOUND;
				break;
			}
		}
	}
	endrpcent();
	return rv;
}
#endif /* !defined(__minix) */

		/*
		 * services
		 */

static int
services(int argc, char *argv[])
{
	struct servent	*se;
	unsigned long	id;
	char		*proto;
	int		i, rv;

	assert(argc > 1);
	assert(argv != NULL);

#define SERVICESPRINT	printfmtstrings(se->s_aliases, "  ", " ", \
			    "%-16s  %5d/%s", \
			    se->s_name, ntohs(se->s_port), se->s_proto)

	setservent(1);
	rv = RV_OK;
	if (argc == 2) {
		while ((se = getservent()) != NULL)
			SERVICESPRINT;
	} else {
		for (i = 2; i < argc; i++) {
			proto = strchr(argv[i], '/');
			if (proto != NULL)
				*proto++ = '\0';
			if (parsenum(argv[i], &id))
				se = getservbyport(htons(id), proto);
			else
				se = getservbyname(argv[i], proto);
			if (se != NULL)
				SERVICESPRINT;
			else {
				rv = RV_NOTFOUND;
				break;
			}
		}
	}
	endservent();
	return rv;
}


		/*
		 * shells
		 */

static int
shells(int argc, char *argv[])
{
	const char	*sh;
	int		i, rv;

	assert(argc > 1);
	assert(argv != NULL);

#define SHELLSPRINT	(void)printf("%s\n", sh)

	setusershell();
	rv = RV_OK;
	if (argc == 2) {
		while ((sh = getusershell()) != NULL)
			SHELLSPRINT;
	} else {
		for (i = 2; i < argc; i++) {
			setusershell();
			while ((sh = getusershell()) != NULL) {
				if (strcmp(sh, argv[i]) == 0) {
					SHELLSPRINT;
					break;
				}
			}
			if (sh == NULL) {
				rv = RV_NOTFOUND;
				break;
			}
		}
	}
	endusershell();
	return rv;
}
