/*	$NetBSD: sysctl.c,v 1.156 2015/08/17 06:42:46 knakahara Exp $ */

/*-
 * Copyright (c) 2003 The NetBSD Foundation, Inc.
 *	All rights reserved.
 *
 * This code is derived from software contributed to The NetBSD Foundation
 * by Andrew Brown.
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

/*
 * Copyright (c) 1993
 *	The Regents of the University of California.  All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 3. Neither the name of the University nor the names of its contributors
 *    may be used to endorse or promote products derived from this software
 *    without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE REGENTS AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE REGENTS OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 */

#include <sys/cdefs.h>
#ifndef lint
__COPYRIGHT("@(#) Copyright (c) 1993\
 The Regents of the University of California.  All rights reserved.");
#endif /* not lint */

#ifndef lint
#if 0
static char sccsid[] = "@(#)sysctl.c	8.1 (Berkeley) 6/6/93";
#else
__RCSID("$NetBSD: sysctl.c,v 1.156 2015/08/17 06:42:46 knakahara Exp $");
#endif
#endif /* not lint */

#include <sys/types.h>
#include <sys/param.h>
#include <sys/sysctl.h>
#include <sys/mount.h>
#include <sys/resource.h>
#include <sys/stat.h>
#include <sys/sched.h>
#include <sys/socket.h>
#include <sys/bitops.h>
#include <netinet/in.h>
#include <netinet/ip_var.h>
#include <netinet/tcp.h>
#ifndef __minix
#include <netinet/tcp_timer.h>
#include <netinet/tcp_var.h>
#include <netinet/icmp6.h>
#include <nfs/rpcv2.h>
#include <nfs/nfsproto.h>
#include <nfs/nfs.h>
#endif /* !__minix */
#include <machine/cpu.h>

#include <assert.h>
#include <ctype.h>
#include <err.h>
#include <errno.h>
#include <inttypes.h>
#include <regex.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <unistd.h>

#include "prog_ops.h"

/*
 * this needs to be able to do the printing and the setting
 */
#define HANDLER_PROTO const char *, const char *, char *, \
	int *, u_int, const struct sysctlnode *, \
	u_int, void *
#define HANDLER_ARGS const char *sname, const char *dname, char *value, \
	int *name, u_int namelen, const struct sysctlnode *pnode, \
	u_int type, void *v
#define DISPLAY_VALUE	0
#define DISPLAY_OLD	1
#define DISPLAY_NEW	2

/*
 * generic routines
 */
static const struct handlespec *findhandler(const char *, regex_t *, size_t *);
static void canonicalize(const char *, char *);
static void purge_tree(struct sysctlnode *);
static void print_tree(int *, u_int, struct sysctlnode *, u_int, int, regex_t *,
    size_t *);
static void write_number(int *, u_int, struct sysctlnode *, char *);
static void write_string(int *, u_int, struct sysctlnode *, char *);
static void display_number(const struct sysctlnode *, const char *,
			   const void *, size_t, int);
static void display_string(const struct sysctlnode *, const char *,
			   const void *, size_t, int);
static void display_struct(const struct sysctlnode *, const char *,
			   const void *, size_t, int);
static void hex_dump(const unsigned char *, size_t);
__dead static void usage(void);
static void parse(char *, regex_t *, size_t *);
static void parse_create(char *);
static void parse_destroy(char *);
static void parse_describe(char *);
static void getdesc1(int *, u_int, struct sysctlnode *);
static void getdesc(int *, u_int, struct sysctlnode *);
static void trim_whitespace(char *, int);
static void sysctlerror(int);
static void sysctlparseerror(u_int, const char *);
static void sysctlperror(const char *, ...) __printflike(1, 2);
#define EXIT(n) do { \
	if (fn == NULL) exit(n); else return; } while (/*CONSTCOND*/0)

/*
 * "borrowed" from libc:sysctlgetmibinfo.c
 */
int __learn_tree(int *, u_int, struct sysctlnode *);

/*
 * "handlers"
 */
static void printother(HANDLER_PROTO);
static void kern_clockrate(HANDLER_PROTO);
static void kern_boottime(HANDLER_PROTO);
static void kern_consdev(HANDLER_PROTO);
static void kern_cp_time(HANDLER_PROTO);
static void kern_cp_id(HANDLER_PROTO);
static void kern_drivers(HANDLER_PROTO);
static void vm_loadavg(HANDLER_PROTO);
static void proc_limit(HANDLER_PROTO);
#ifdef CPU_DISKINFO
static void machdep_diskinfo(HANDLER_PROTO);
#endif /* CPU_DISKINFO */
static void mode_bits(HANDLER_PROTO);
static void reserve(HANDLER_PROTO);

static const struct handlespec {
	const char *ps_re;
	void (*ps_p)(HANDLER_PROTO);
	void (*ps_w)(HANDLER_PROTO);
	const void *ps_d;
} handlers[] = {
	{ "/kern/clockrate",			kern_clockrate, NULL, NULL },
	{ "/kern/evcnt",			printother, NULL, "vmstat -e" },
	{ "/kern/vnode",			printother, NULL, "pstat" },
	{ "/kern/proc(2|_args)?",		printother, NULL, "ps" },
	{ "/kern/file2?",			printother, NULL, "pstat" },
	{ "/kern/ntptime",			printother, NULL,
						"ntpdc -c kerninfo" },
	{ "/kern/msgbuf",			printother, NULL, "dmesg" },
	{ "/kern/boottime",			kern_boottime, NULL, NULL },
	{ "/kern/consdev",			kern_consdev, NULL, NULL },
	{ "/kern/cp_time(/[0-9]+)?",		kern_cp_time, NULL, NULL },
	{ "/kern/sysvipc_info",			printother, NULL, "ipcs" },
	{ "/kern/cp_id(/[0-9]+)?",		kern_cp_id, NULL, NULL },

	{ "/kern/coredump/setid/mode",		mode_bits, mode_bits, NULL },
	{ "/kern/drivers",			kern_drivers, NULL, NULL },

	{ "/kern/intr/list",			printother, NULL, "intrctl" },
	{ "/kern/intr/affinity",		printother, NULL, "intrctl" },
	{ "/kern/intr/intr",			printother, NULL, "intrctl" },
	{ "/kern/intr/nointr",			printother, NULL, "intrctl" },

	{ "/vm/vmmeter",			printother, NULL,
						"vmstat' or 'systat" },
	{ "/vm/loadavg",			vm_loadavg, NULL, NULL },
	{ "/vm/uvmexp2?",			printother, NULL,
						"vmstat' or 'systat" },

	{ "/vfs/nfs/nfsstats",			printother, NULL, "nfsstat" },

	{ "/net/inet6?/tcp6?/ident",		printother, NULL, "identd" },
	{ "/net/inet6/icmp6/nd6_[dp]rlist",	printother, NULL, "ndp" },
	{ "/net/key/dumps[ap]",			printother, NULL, "setkey" },
	{ "/net/[^/]+/[^/]+/pcblist",		printother, NULL,
						"netstat' or 'sockstat" },
	{ "/net/(inet|inet6)/[^/]+/stats",	printother, NULL, "netstat"},
	{ "/net/bpf/(stats|peers)",		printother, NULL, "netstat"},

	{ "/net/inet.*/tcp.*/deb.*",		printother, NULL, "trpt" },

	{ "/net/inet.*/ip.*/anonportalgo/reserve", reserve, reserve, NULL },

	{ "/net/ns/spp/deb.*",			printother, NULL, "trsp" },

	{ "/hw/diskstats",			printother, NULL, "iostat" },

#ifdef CPU_CONSDEV
	{ "/machdep/consdev",			kern_consdev, NULL, NULL },
#endif /* CPU_CONSDEV */
#ifdef CPU_DISKINFO
	{ "/machdep/diskinfo",			machdep_diskinfo, NULL, NULL },
#endif /* CPU_CONSDEV */

	{ "/proc/[^/]+/rlimit/[^/]+/[^/]+",	proc_limit, proc_limit, NULL },

	/*
	 * these will only be called when the given node has no children
	 */
	{ "/net/[^/]+",				printother, NULL, NULL },
	{ "/debug",				printother, NULL, NULL },
	{ "/ddb",				printother, NULL, NULL },
	{ "/vendor",				printother, NULL, NULL },

	{ NULL,					NULL, NULL, NULL },
};

struct sysctlnode my_root = {
	.sysctl_flags = SYSCTL_VERSION|CTLFLAG_ROOT|CTLTYPE_NODE,
	.sysctl_size = sizeof(struct sysctlnode),
	.sysctl_num = 0,
	.sysctl_name = "(prog_root)",
};

int	Aflag, aflag, dflag, Mflag, nflag, qflag, rflag, wflag, xflag;
size_t	nr;
char	*fn;
int	req, stale, errs;
FILE	*warnfp = stderr;

#define MAXPORTS	0x10000

/*
 * vah-riables n stuff
 */
char gsname[SYSCTL_NAMELEN * CTL_MAXNAME + CTL_MAXNAME],
	canonname[SYSCTL_NAMELEN * CTL_MAXNAME + CTL_MAXNAME],
	gdname[10 * CTL_MAXNAME + CTL_MAXNAME];
char sep[] = ".";
const char *eq = " = ";
const char *lname[] = {
	"top", "second", "third", "fourth", "fifth", "sixth",
	"seventh", "eighth", "ninth", "tenth", "eleventh", "twelfth"
};

/*
 * you've heard of main, haven't you?
 */
int
main(int argc, char *argv[])
{
	int name[CTL_MAXNAME];
	int ch;
	size_t lastcompiled = 0;
	regex_t *re;

	setprogname(argv[0]);
	while ((ch = getopt(argc, argv, "Aabdef:Mnqrwx")) != -1) {
		switch (ch) {
		case 'A':
			Aflag++;
			break;
		case 'a':
			aflag++;
			break;
		case 'd':
			dflag++;
			break;
		case 'e':
			eq = "=";
			break;
		case 'f':
			fn = optarg;
			wflag++;
			break;
		case 'M':
			Mflag++;
			break;
		case 'n':
			nflag++;
			break;
		case 'q':
			qflag++;
			break;
		case 'b':	/* FreeBSD compat */
		case 'r':
			rflag++; 
			break;
		case 'w':
			wflag++;
			break;
		case 'x':
			xflag++;
			break;
		default:
			usage();
		}
	}

	argc -= optind;
	argv += optind;

	if (xflag && rflag)
		usage();
	/* if ((xflag || rflag) && wflag)
		usage(); */
	/* if (aflag && (Mflag || qflag))
		usage(); */
	if ((aflag || Aflag) && qflag)
		usage();
	if ((Aflag || Mflag || dflag) && argc == 0 && fn == NULL)
		aflag = 1;

	if (prog_init && prog_init() == -1)
		err(EXIT_FAILURE, "prog init failed");

	if (Aflag)
		warnfp = stdout;
	stale = req = 0;

	if ((re = malloc(sizeof(*re) * __arraycount(handlers))) == NULL)
		err(EXIT_FAILURE, "malloc regex");

	if (aflag) {
		print_tree(&name[0], 0, NULL, CTLTYPE_NODE, 1,
		    re, &lastcompiled);
		/* if (argc == 0) */
		return (0);
	}

	if (fn) {
		FILE *fp;
		char *l;

		fp = fopen(fn, "r");
		if (fp == NULL) {
			err(EXIT_FAILURE, "%s", fn);
		} else {
			nr = 0;
			while ((l = fparseln(fp, NULL, &nr, NULL, 0)) != NULL)
			{
				if (*l) {
					parse(l, re, &lastcompiled);
					free(l);
				}
			}
			fclose(fp);
		}
		return errs ? 1 : 0;
	}

	if (argc == 0)
		usage();

	while (argc-- > 0)
		parse(*argv++, re, &lastcompiled);

	return errs ? EXIT_FAILURE : EXIT_SUCCESS;
}

/*
 * ********************************************************************
 * how to find someone special to handle the reading (or maybe even
 * writing) of a particular node
 * ********************************************************************
 */
static const struct handlespec *
findhandler(const char *s, regex_t *re, size_t *lastcompiled)
{
	const struct handlespec *p;
	size_t i, l;
	int j;
	char eb[64];
	regmatch_t match;

	p = &handlers[0];
	l = strlen(s);
	for (i = 0; p[i].ps_re != NULL; i++) {
		if (i >= *lastcompiled) {
			j = regcomp(&re[i], p[i].ps_re, REG_EXTENDED);
			if (j != 0) {
				regerror(j, &re[i], eb, sizeof(eb));
				errx(EXIT_FAILURE, "regcomp: %s: %s", p[i].ps_re, eb);
			}
			*lastcompiled = i + 1;
		}
		j = regexec(&re[i], s, 1, &match, 0);
		if (j == 0) {
			if (match.rm_so == 0 && match.rm_eo == (int)l)
				return &p[i];
		}
		else if (j != REG_NOMATCH) {
			regerror(j, &re[i], eb, sizeof(eb));
			errx(EXIT_FAILURE, "regexec: %s: %s", p[i].ps_re, eb);
		}
	}

	return NULL;
}

/*
 * after sysctlgetmibinfo is done with the name, we convert all
 * separators to / and stuff one at the front if it was missing
 */
static void
canonicalize(const char *i, char *o)
{
	const char *t;
	char p[SYSCTL_NAMELEN + 1];
	int l;

	if (i[0] != *sep) {
		o[0] = '/';
		o[1] = '\0';
	}
	else
		o[0] = '\0';

	t = i;
	do {
		i = t;
		t = strchr(i, sep[0]);
		if (t == NULL)
			strcat(o, i);
		else {
			l = t - i;
			t++;
			memcpy(p, i, l);
			p[l] = '\0';
			strcat(o, p);
			strcat(o, "/");
		}
	} while (t != NULL);
}

/*
 * ********************************************************************
 * convert this special number to a special string so we can print the
 * mib
 * ********************************************************************
 */
static const char *
sf(u_int f)
{
	static char s[256];
	const char *c;

	s[0] = '\0';
	c = "";

#define print_flag(_f, _s, _c, _q, _x) \
	if (((_f) & (__CONCAT(CTLFLAG_,_x))) == (__CONCAT(CTLFLAG_,_q))) { \
		strlcat((_s), (_c), sizeof(_s)); \
		strlcat((_s), __STRING(_q), sizeof(_s)); \
		(_c) = ","; \
		(_f) &= ~(__CONCAT(CTLFLAG_,_x)); \
	}
	print_flag(f, s, c, READONLY,  READWRITE);
	print_flag(f, s, c, READWRITE, READWRITE);
	print_flag(f, s, c, ANYWRITE,  ANYWRITE);
	print_flag(f, s, c, PRIVATE,   PRIVATE);
	print_flag(f, s, c, PERMANENT, PERMANENT);
	print_flag(f, s, c, OWNDATA,   OWNDATA);
	print_flag(f, s, c, IMMEDIATE, IMMEDIATE);
	print_flag(f, s, c, HEX,       HEX);
	print_flag(f, s, c, ROOT,      ROOT);
	print_flag(f, s, c, ANYNUMBER, ANYNUMBER);
	print_flag(f, s, c, HIDDEN,    HIDDEN);
	print_flag(f, s, c, ALIAS,     ALIAS);
#undef print_flag

	if (f) {
		char foo[9];
		snprintf(foo, sizeof(foo), "%x", f);
		strlcat(s, c, sizeof(s));
		strlcat(s, foo, sizeof(s));
	}

	return (s);
}

static const char *
st(u_int t)
{

	switch (t) {
	case CTLTYPE_NODE:
		return "NODE";
	case CTLTYPE_INT:
		return "INT";
	case CTLTYPE_STRING:
		return "STRING";
	case CTLTYPE_QUAD:
                return "QUAD";
	case CTLTYPE_STRUCT:
		return "STRUCT";
	case CTLTYPE_BOOL:
		return "BOOL";
	}

	return "???";
}

/*
 * ********************************************************************
 * recursively eliminate all data belonging to the given node
 * ********************************************************************
 */
static void
purge_tree(struct sysctlnode *rnode)
{
	struct sysctlnode *node;

	if (rnode == NULL ||
	    SYSCTL_TYPE(rnode->sysctl_flags) != CTLTYPE_NODE ||
	    rnode->sysctl_child == NULL)
		return;

	for (node = rnode->sysctl_child;
	     node < &rnode->sysctl_child[rnode->sysctl_clen];
	     node++)
		purge_tree(node);
	free(rnode->sysctl_child);
	rnode->sysctl_csize = 0;
	rnode->sysctl_clen = 0;
	rnode->sysctl_child = NULL;

	if (rnode->sysctl_desc == (const char*)-1)
		rnode->sysctl_desc = NULL;
	if (rnode->sysctl_desc != NULL)
		free(__UNCONST(rnode->sysctl_desc));
	rnode->sysctl_desc = NULL;
}

static void __attribute__((__format__(__printf__, 3, 4)))
appendprintf(char **bp, size_t *lbp, const char *fmt, ...)
{
	int r;
	va_list ap;

	va_start(ap, fmt);
	r = vsnprintf(*bp, *lbp, fmt, ap);
	va_end(ap);
	if (r < 0 || (size_t)r > *lbp)
		r = *lbp;
	*bp += r;
	*lbp -= r;
}

/*
 * ********************************************************************
 * print this node and any others underneath it
 * ********************************************************************
 */
static void
print_tree(int *name, u_int namelen, struct sysctlnode *pnode, u_int type,
   int add, regex_t *re, size_t *lastcompiled)
{
	struct sysctlnode *node;
	int rc;
	size_t ni, sz, ldp, lsp;
	char *sp, *dp, *tsp, *tdp;
	const struct handlespec *p;

	sp = tsp = &gsname[strlen(gsname)];
	dp = tdp = &gdname[strlen(gdname)];
	ldp = sizeof(gdname) - (dp - gdname);
	lsp = sizeof(gsname) - (sp - gsname);

	if (sp != &gsname[0] && dp == &gdname[0]) {
		/*
		 * aw...shucks.  now we must play catch up
		 */
		for (ni = 0; ni < namelen; ni++)
			appendprintf(&tdp, &ldp, "%s%d", ni > 0 ? "." : "",
			    name[ni]);
	}

	if (pnode == NULL)
		pnode = &my_root;
	else if (add) {
		appendprintf(&tsp, &lsp, "%s%s", namelen > 1 ? sep : "", 
			pnode->sysctl_name);
		appendprintf(&tdp, &ldp, "%s%d", namelen > 1 ? "." : "", 
			pnode->sysctl_num);
	}

	if (Mflag && pnode != &my_root) {
		if (nflag)
			printf("%s: ", gdname);
		else
			printf("%s (%s): ", gsname, gdname);
		printf("CTLTYPE_%s", st(type));
		if (type == CTLTYPE_NODE) {
			if (SYSCTL_FLAGS(pnode->sysctl_flags) & CTLFLAG_ALIAS)
				printf(", alias %d",
				       pnode->sysctl_alias);
			else
				printf(", children %d/%d",
				       pnode->sysctl_clen,
				       pnode->sysctl_csize);
		}
		printf(", size %zu", pnode->sysctl_size);
		printf(", flags 0x%x<%s>",
		       SYSCTL_FLAGS(pnode->sysctl_flags),
		       sf(SYSCTL_FLAGS(pnode->sysctl_flags)));
		if (pnode->sysctl_func)
			printf(", func=%p", pnode->sysctl_func);
		printf(", ver=%d", pnode->sysctl_ver);
		printf("\n");
		if (type != CTLTYPE_NODE) {
			*sp = *dp = '\0';
			return;
		}
	}

	if (dflag && pnode != &my_root) {
		if (Aflag || type != CTLTYPE_NODE) {
			if (pnode->sysctl_desc == NULL)
				getdesc1(name, namelen, pnode);
			if (Aflag || !add ||
			    (pnode->sysctl_desc != NULL &&
			     pnode->sysctl_desc != (const char*)-1)) {
				if (!nflag)
					printf("%s: ", gsname);
				if (pnode->sysctl_desc == NULL ||
				    pnode->sysctl_desc == (const char*)-1)
					printf("(no description)\n");
				else
					printf("%s\n", pnode->sysctl_desc);
			}
		}

		if (type != CTLTYPE_NODE) {
			*sp = *dp = '\0';
			return;
		}
	}

	/*
	 * if this is an alias and we added our name, that means we
	 * got here by recursing down into the tree, so skip it.  The
	 * only way to print an aliased node is with either -M or by
	 * name specifically.
	 */
	if (SYSCTL_FLAGS(pnode->sysctl_flags) & CTLFLAG_ALIAS && add) {
		*sp = *dp = '\0';
		return;
	}

	canonicalize(gsname, canonname);
	p = findhandler(canonname, re, lastcompiled);
	if (type != CTLTYPE_NODE && p != NULL) {
		if (p->ps_p == NULL) {
			sysctlperror("Cannot print `%s': %s\n", gsname, 
			    strerror(EOPNOTSUPP));
			exit(EXIT_FAILURE);
		}
		(*p->ps_p)(gsname, gdname, NULL, name, namelen, pnode, type,
			   __UNCONST(p->ps_d));
		*sp = *dp = '\0';
		return;
	}

	if (type != CTLTYPE_NODE && pnode->sysctl_size == 0) {
		rc = prog_sysctl(&name[0], namelen, NULL, &sz, NULL, 0);
		if (rc == -1) {
			sysctlerror(1);
			*sp = *dp = '\0';
			return;
		}
		if (sz == 0) {
			if ((Aflag || req) && !Mflag)
				printf("%s: node contains no data\n", gsname);
			*sp = *dp = '\0';
			return;
		}
	}
	else
		sz = pnode->sysctl_size;

	switch (type) {
	case CTLTYPE_NODE: {
		__learn_tree(name, namelen, pnode);
		node = pnode->sysctl_child;
		if (node == NULL) {
			if (dflag)
				/* do nothing */;
			else if (p != NULL)
				(*p->ps_p)(gsname, gdname, NULL, name, namelen,
					   pnode, type, __UNCONST(p->ps_d));
			else if ((Aflag || req) && !Mflag)
				printf("%s: no children\n", gsname);
		}
		else {
			if (dflag)
				/*
				 * get all descriptions for next level
				 * in one chunk
				 */
				getdesc(name, namelen, pnode);
			req = 0;
			for (ni = 0; ni < pnode->sysctl_clen; ni++) {
				name[namelen] = node[ni].sysctl_num;
				if ((node[ni].sysctl_flags & CTLFLAG_HIDDEN) &&
				    !(Aflag || req))
					continue;
				print_tree(name, namelen + 1, &node[ni],
					   SYSCTL_TYPE(node[ni].sysctl_flags),
					   1, re, lastcompiled);
			}
		}
		break;
	}
	case CTLTYPE_INT: {
		int i;
		rc = prog_sysctl(name, namelen, &i, &sz, NULL, 0);
		if (rc == -1) {
			sysctlerror(1);
			break;
		}
		display_number(pnode, gsname, &i, sizeof(i), DISPLAY_VALUE);
		break;
	}
	case CTLTYPE_BOOL: {
		bool b;
		rc = prog_sysctl(name, namelen, &b, &sz, NULL, 0);
		if (rc == -1) {
			sysctlerror(1);
			break;
		}
		display_number(pnode, gsname, &b, sizeof(b), DISPLAY_VALUE);
		break;
	}
	case CTLTYPE_STRING: {
		unsigned char buf[1024], *tbuf;
		tbuf = buf;
		sz = sizeof(buf);
		rc = prog_sysctl(&name[0], namelen, tbuf, &sz, NULL, 0);
		if (rc == -1 && errno == ENOMEM) {
			tbuf = malloc(sz);
			if (tbuf == NULL) {
				sysctlerror(1);
				break;
			}
			rc = prog_sysctl(&name[0], namelen, tbuf, &sz, NULL, 0);
		}
		if (rc == -1)
			sysctlerror(1);
		else
			display_string(pnode, gsname, tbuf, sz, DISPLAY_VALUE);
		if (tbuf != buf)
			free(tbuf);
		break;
	}
	case CTLTYPE_QUAD: {
		u_quad_t q;
		sz = sizeof(q);
		rc = prog_sysctl(&name[0], namelen, &q, &sz, NULL, 0);
		if (rc == -1) {
			sysctlerror(1);
			break;
		}
		display_number(pnode, gsname, &q, sizeof(q), DISPLAY_VALUE);
		break;
	}
	case CTLTYPE_STRUCT: {
		/*
		 * we shouldn't actually get here, but if we
		 * do, would it be nice to have *something* to
		 * do other than completely ignore the
		 * request.
		 */
		unsigned char *d;
		if ((d = malloc(sz)) == NULL) {
			fprintf(warnfp, "%s: !malloc failed!\n", gsname);
			break;
		}
		rc = prog_sysctl(&name[0], namelen, d, &sz, NULL, 0);
		if (rc == -1) {
			sysctlerror(1);
			break;
		}
		display_struct(pnode, gsname, d, sz, DISPLAY_VALUE);
		free(d);
		break;
	}
	default:
		/* should i print an error here? */
		break;
	}

	*sp = *dp = '\0';
}

/*
 * ********************************************************************
 * parse a request, possibly determining that it's a create or destroy
 * request
 * ********************************************************************
 */
static void
parse(char *l, regex_t *re, size_t *lastcompiled)
{
	struct sysctlnode *node;
	const struct handlespec *w;
	int name[CTL_MAXNAME], dodesc = 0;
	u_int namelen, type;
	char *key, *value, *dot;
	size_t sz;
	bool optional = false;

	req = 1;
	key = l;

	if ((value = strchr(l, '=')) != NULL) {
		if (value > l && value[-1] == '?') {
			value[-1] = '\0';
			optional = true;
		}
		*value++ = '\0';
	}

	if ((dot = strpbrk(key, "./")) == NULL)
		sep[0] = '.';
	else
		sep[0] = dot[0];
	sep[1] = '\0';

	while (key[0] == sep[0] && key[1] == sep[0]) {
		if (value != NULL)
			value[-1] = '=';
		if (strncmp(key + 2, "create", 6) == 0 &&
		    (key[8] == '=' || key[8] == sep[0]))
			parse_create(key + 8 + (key[8] == '=' ? 1 : 0));
		else if (strncmp(key + 2, "destroy", 7) == 0 &&
			 (key[9] == '=' || key[9] == sep[0]))
			parse_destroy(key + 9 + (key[9] == '=' ? 1 : 0));
		else if (strncmp(key + 2, "describe", 8) == 0 &&
			 (key[10] == '=' || key[10] == sep[0])) {
			key += 10 + (key[10] == '=');
			if ((value = strchr(key, '=')) != NULL)
				parse_describe(key);
			else {
				if (!dflag)
					dodesc = 1;
				break;
			}
		}
		else
			sysctlperror("unable to parse '%s'\n", key);
		return;
	}

	if (stale) {
		purge_tree(&my_root);
		stale = 0;
	}
	node = &my_root;
	namelen = CTL_MAXNAME;
	sz = sizeof(gsname);

	if (sysctlgetmibinfo(key, &name[0], &namelen, gsname, &sz, &node,
			     SYSCTL_VERSION) == -1) {
		if (optional)
			return;
		sysctlparseerror(namelen, l);
		EXIT(EXIT_FAILURE);
	}

	type = SYSCTL_TYPE(node->sysctl_flags);

	if (value == NULL) {
		if (dodesc)
			dflag = 1;
		print_tree(&name[0], namelen, node, type, 0, re, lastcompiled);
		if (dodesc)
			dflag = 0;
		gsname[0] = '\0';
		return;
	}

	if (fn)
		trim_whitespace(value, 1);

	if (!wflag) {
		sysctlperror("Must specify -w to set variables\n");
		exit(EXIT_FAILURE);
	}

	canonicalize(gsname, canonname);
	if (type != CTLTYPE_NODE && (w = findhandler(canonname, re,
	    lastcompiled)) != NULL) {
		if (w->ps_w == NULL) {
			sysctlperror("Cannot write `%s': %s\n", gsname, 
			    strerror(EOPNOTSUPP));
			exit(EXIT_FAILURE);
		}
		(*w->ps_w)(gsname, gdname, value, name, namelen, node, type,
			   __UNCONST(w->ps_d));
		gsname[0] = '\0';
		return;
	}

	switch (type) {
	case CTLTYPE_NODE:
		/*
		 * XXX old behavior is to print.  should we error instead?
		 */
		print_tree(&name[0], namelen, node, CTLTYPE_NODE, 1, re,
		    lastcompiled);
		break;
	case CTLTYPE_INT:
	case CTLTYPE_BOOL:
	case CTLTYPE_QUAD:
		write_number(&name[0], namelen, node, value);
		break;
	case CTLTYPE_STRING:
		write_string(&name[0], namelen, node, value);
		break;
	case CTLTYPE_STRUCT:
		/*
		 * XXX old behavior is to print.  should we error instead?
		 */
		/* fprintf(warnfp, "you can't write to %s\n", gsname); */
		print_tree(&name[0], namelen, node, type, 0, re, lastcompiled);
		break;
	}
}

/*

  //create=foo.bar.whatever...,
  [type=(int|quad|string|struct|node),]
  [size=###,]
  [n=###,]
  [flags=(iohxparw12),]
  [addr=0x####,|symbol=...|value=...]

  size is optional for some types.  type must be set before anything
  else.  nodes can have [rwhp], but nothing else applies.  if no
  size or type is given, node is asserted.  writeable is the default,
  with [rw] being read-only and unconditionally writeable
  respectively.  if you specify addr, it is assumed to be the name of
  a kernel symbol, if value, CTLFLAG_OWNDATA will be asserted for
  strings, CTLFLAG_IMMEDIATE for ints and u_quad_ts.  you cannot
  specify both value and addr.

*/

static void
parse_create(char *l)
{
	struct sysctlnode node;
	size_t sz;
	char *nname, *key, *value, *data, *addr, *c, *t;
	int name[CTL_MAXNAME], i, rc, method, flags, rw;
	u_int namelen, type;
	u_quad_t uq;
	quad_t q;
	bool b;

	if (!wflag) {
		sysctlperror("Must specify -w to create nodes\n");
		exit(EXIT_FAILURE);
	}

	/*
	 * these are the pieces that make up the description of a new
	 * node
	 */
	memset(&node, 0, sizeof(node));
	node.sysctl_num = CTL_CREATE;  /* any number is fine */
	flags = 0;
	rw = -1;
	type = 0;
	sz = 0;
	data = addr = NULL;
	memset(name, 0, sizeof(name));
	namelen = 0;
	method = 0;

	/*
	 * misc stuff used when constructing
	 */
	i = 0;
	b = false;
	uq = 0;
	key = NULL;
	value = NULL;

	/*
	 * the name of the thing we're trying to create is first, so
	 * pick it off.
	 */
	nname = l;
	if ((c = strchr(nname, ',')) != NULL)
		*c++ = '\0';

	while (c != NULL) {

		/*
		 * pull off the next "key=value" pair
		 */
		key = c;
		if ((t = strchr(key, '=')) != NULL) {
			*t++ = '\0';
			value = t;
		}
		else
			value = NULL;

		/*
		 * if the "key" is "value", then that eats the rest of
		 * the string, so we're done, otherwise bite it off at
		 * the next comma.
		 */
		if (strcmp(key, "value") == 0) {
			c = NULL;
			data = value;
			break;
		}
		else if (value) {
			if ((c = strchr(value, ',')) != NULL)
				*c++ = '\0';
		}

		/*
		 * note that we (mostly) let the invoker of prog_sysctl(8)
		 * play rampant here and depend on the kernel to tell
		 * them that they were wrong.  well...within reason.
		 * we later check the various parameters against each
		 * other to make sure it makes some sort of sense.
		 */
		if (strcmp(key, "addr") == 0) {
			/*
			 * we can't check these two.  only the kernel
			 * can tell us when it fails to find the name
			 * (or if the address is invalid).
			 */
			if (method != 0) {
				sysctlperror(
				    "%s: already have %s for new node\n",
				    nname,
				    method == CTL_CREATE ? "addr" : "symbol");
				EXIT(EXIT_FAILURE);
			}
			if (value == NULL) {
				sysctlperror("%s: missing value\n", nname);
				EXIT(EXIT_FAILURE);
			}
			errno = 0;
			addr = (void*)strtoul(value, &t, 0);
			if (t == value || *t != '\0' || errno != 0) {
				sysctlperror(
				    "%s: '%s' is not a valid address\n",
				    nname, value);
				EXIT(EXIT_FAILURE);
			}
			method = CTL_CREATE;
		}
		else if (strcmp(key, "symbol") == 0) {
			if (method != 0) {
				sysctlperror(
				    "%s: already have %s for new node\n",
				    nname,
				    method == CTL_CREATE ? "addr" : "symbol");
				EXIT(EXIT_FAILURE);
			}
			addr = value;
			method = CTL_CREATESYM;
		}
		else if (strcmp(key, "type") == 0) {
			if (value == NULL) {
				sysctlperror("%s: missing value\n", nname);
				EXIT(EXIT_FAILURE);
			}
			if (strcmp(value, "node") == 0)
				type = CTLTYPE_NODE;
			else if (strcmp(value, "int") == 0) {
				sz = sizeof(int);
				type = CTLTYPE_INT;
			}
			else if (strcmp(value, "bool") == 0) {
				sz = sizeof(bool);
				type = CTLTYPE_BOOL;
			}
			else if (strcmp(value, "string") == 0)
				type = CTLTYPE_STRING;
			else if (strcmp(value, "quad") == 0) {
				sz = sizeof(u_quad_t);
				type = CTLTYPE_QUAD;
			}
			else if (strcmp(value, "struct") == 0)
				type = CTLTYPE_STRUCT;
			else {
				sysctlperror(
					"%s: '%s' is not a valid type\n",
					nname, value);
				EXIT(EXIT_FAILURE);
			}
		}
		else if (strcmp(key, "size") == 0) {
			if (value == NULL) {
				sysctlperror("%s: missing value\n", nname);
				EXIT(EXIT_FAILURE);
			}
			errno = 0;
			/*
			 * yes, i know size_t is not an unsigned long,
			 * but we can all agree that it ought to be,
			 * right?
			 */
			sz = strtoul(value, &t, 0);
			if (t == value || *t != '\0' || errno != 0) {
				sysctlperror(
					"%s: '%s' is not a valid size\n",
					nname, value);
				EXIT(EXIT_FAILURE);
			}
		}
		else if (strcmp(key, "n") == 0) {
			if (value == NULL) {
				sysctlperror("%s: missing value\n", nname);
				EXIT(EXIT_FAILURE);
			}
			errno = 0;
			q = strtoll(value, &t, 0);
			if (t == value || *t != '\0' || errno != 0 ||
			    q < INT_MIN || q > UINT_MAX) {
				sysctlperror(
				    "%s: '%s' is not a valid mib number\n",
				    nname, value);
				EXIT(EXIT_FAILURE);
			}
			node.sysctl_num = (int)q;
		}
		else if (strcmp(key, "flags") == 0) {
			if (value == NULL) {
				sysctlperror("%s: missing value\n", nname);
				EXIT(EXIT_FAILURE);
			}
			t = value;
			while (*t != '\0') {
				switch (*t) {
				case 'a':
					flags |= CTLFLAG_ANYWRITE;
					break;
				case 'h':
					flags |= CTLFLAG_HIDDEN;
					break;
				case 'i':
					flags |= CTLFLAG_IMMEDIATE;
					break;
				case 'o':
					flags |= CTLFLAG_OWNDATA;
					break;
				case 'p':
					flags |= CTLFLAG_PRIVATE;
					break;
				case 'u':
					flags |= CTLFLAG_UNSIGNED;
					break;
				case 'x':
					flags |= CTLFLAG_HEX;
					break;

				case 'r':
					rw = CTLFLAG_READONLY;
					break;
				case 'w':
					rw = CTLFLAG_READWRITE;
					break;
				default:
					sysctlperror(
					   "%s: '%c' is not a valid flag\n",
					    nname, *t);
					EXIT(EXIT_FAILURE);
				}
				t++;
			}
		}
		else {
			sysctlperror("%s: unrecognized keyword '%s'\n",
				     nname, key);
			EXIT(EXIT_FAILURE);
		}
	}

	/*
	 * now that we've finished parsing the given string, fill in
	 * anything they didn't specify
	 */
	if (type == 0)
		type = CTLTYPE_NODE;

	/*
	 * the "data" can be interpreted various ways depending on the
	 * type of node we're creating, as can the size
	 */
	if (data != NULL) {
		if (addr != NULL) {
			sysctlperror(
				"%s: cannot specify both value and "
				"address\n", nname);
			EXIT(EXIT_FAILURE);
		}

		switch (type) {
		case CTLTYPE_INT:
			errno = 0;
			q = strtoll(data, &t, 0);
			if (t == data || *t != '\0' || errno != 0 ||
				q < INT_MIN || q > UINT_MAX) {
				sysctlperror(
				    "%s: '%s' is not a valid integer\n",
				    nname, value);
				EXIT(EXIT_FAILURE);
			}
			i = (int)q;
			if (!(flags & CTLFLAG_OWNDATA)) {
				flags |= CTLFLAG_IMMEDIATE;
				node.sysctl_idata = i;
			}
			else
				node.sysctl_data = &i;
			if (sz == 0)
				sz = sizeof(int);
			break;
		case CTLTYPE_BOOL:
			errno = 0;
			q = strtoll(data, &t, 0);
			if (t == data || *t != '\0' || errno != 0 ||
				(q != 0 && q != 1)) {
				sysctlperror(
				    "%s: '%s' is not a valid bool\n",
				    nname, value);
				EXIT(EXIT_FAILURE);
			}
			b = q == 1;
			if (!(flags & CTLFLAG_OWNDATA)) {
				flags |= CTLFLAG_IMMEDIATE;
				node.sysctl_idata = b;
			}
			else
				node.sysctl_data = &b;
			if (sz == 0)
				sz = sizeof(bool);
			break;
		case CTLTYPE_STRING:
			flags |= CTLFLAG_OWNDATA;
			node.sysctl_data = data;
			if (sz == 0)
				sz = strlen(data) + 1;
			else if (sz < strlen(data) + 1) {
				sysctlperror("%s: ignoring size=%zu for "
					"string node, too small for given "
					"value\n", nname, sz);
				sz = strlen(data) + 1;
			}
			break;
		case CTLTYPE_QUAD:
			errno = 0;
			uq = strtouq(data, &t, 0);
			if (t == data || *t != '\0' || errno != 0) {
				sysctlperror(
					"%s: '%s' is not a valid quad\n",
					nname, value);
				EXIT(EXIT_FAILURE);
			}
			if (!(flags & CTLFLAG_OWNDATA)) {
				flags |= CTLFLAG_IMMEDIATE;
				node.sysctl_qdata = uq;
			}
			else
				node.sysctl_data = &uq;
			if (sz == 0)
				sz = sizeof(u_quad_t);
			break;
		case CTLTYPE_STRUCT:
			sysctlperror("%s: struct not initializable\n",
				     nname);
			EXIT(EXIT_FAILURE);
		}

		/*
		 * these methods have all provided local starting
		 * values that the kernel must copy in
		 */
	}

	/*
	 * hmm...no data, but we have an address of data.  that's
	 * fine.
	 */
	else if (addr != 0)
		node.sysctl_data = (void*)addr;

	/*
	 * no data and no address?  well...okay.  we might be able to
	 * manage that.
	 */
	else if (type != CTLTYPE_NODE) {
		if (sz == 0) {
			sysctlperror(
			    "%s: need a size or a starting value\n",
			    nname);
                        EXIT(EXIT_FAILURE);
                }
		if (!(flags & CTLFLAG_IMMEDIATE))
			flags |= CTLFLAG_OWNDATA;
	}

	/*
	 * now we do a few sanity checks on the description we've
	 * assembled
	 */
	if ((flags & CTLFLAG_IMMEDIATE) &&
	    (type == CTLTYPE_STRING || type == CTLTYPE_STRUCT)) {
		sysctlperror("%s: cannot make an immediate %s\n", 
			     nname,
			     (type == CTLTYPE_STRING) ? "string" : "struct");
		EXIT(EXIT_FAILURE);
	}
	if (type == CTLTYPE_NODE && node.sysctl_data != NULL) {
		sysctlperror("%s: nodes do not have data\n", nname);
		EXIT(EXIT_FAILURE);
	}
	
	/*
	 * some types must have a particular size
	 */
	if (sz != 0) {
		if ((type == CTLTYPE_INT && sz != sizeof(int)) ||
		    (type == CTLTYPE_BOOL && sz != sizeof(bool)) ||
		    (type == CTLTYPE_QUAD && sz != sizeof(u_quad_t)) ||
		    (type == CTLTYPE_NODE && sz != 0)) {
			sysctlperror("%s: wrong size for type\n", nname);
			EXIT(EXIT_FAILURE);
		}
	}
	else if (type == CTLTYPE_STRUCT) {
		sysctlperror("%s: struct must have size\n", nname);
		EXIT(EXIT_FAILURE);
	}

	/*
	 * now...if no one said anything yet, we default nodes or
	 * any type that owns data being writeable, and everything
	 * else being readonly.
	 */
	if (rw == -1) {
		if (type == CTLTYPE_NODE ||
		    (flags & (CTLFLAG_OWNDATA|CTLFLAG_IMMEDIATE)))
			rw = CTLFLAG_READWRITE;
		else
			rw = CTLFLAG_READONLY;
	}

	/*
	 * if a kernel address was specified, that can't be made
	 * writeable by us.
	if (rw != CTLFLAG_READONLY && addr) {
		sysctlperror("%s: kernel data can only be readable\n", nname);
		EXIT(EXIT_FAILURE);
	}
	 */

	/*
	 * what separator were they using in the full name of the new
	 * node?
	 */
	if ((t = strpbrk(nname, "./")) == NULL)
		sep[0] = '.';
	else
		sep[0] = t[0];
	sep[1] = '\0';

	/*
	 * put it all together, now.  t'ain't much, is it?
	 */
	node.sysctl_flags = SYSCTL_VERSION|flags|rw|type;
	node.sysctl_size = sz;
	t = strrchr(nname, sep[0]);
	if (t != NULL)
		strlcpy(node.sysctl_name, t + 1, sizeof(node.sysctl_name));
	else
		strlcpy(node.sysctl_name, nname, sizeof(node.sysctl_name));
	if (t == nname)
		t = NULL;

	/*
	 * if this is a new top-level node, then we don't need to find
	 * the mib for its parent
	 */
	if (t == NULL) {
		namelen = 0;
		gsname[0] = '\0';
	}

	/*
	 * on the other hand, if it's not a top-level node...
	 */
	else {
		namelen = sizeof(name) / sizeof(name[0]);
		sz = sizeof(gsname);
		*t = '\0';
		rc = sysctlgetmibinfo(nname, &name[0], &namelen,
				      gsname, &sz, NULL, SYSCTL_VERSION);
		*t = sep[0];
		if (rc == -1) {
			sysctlparseerror(namelen, nname);
			EXIT(EXIT_FAILURE);
		}
	}

	/*
	 * yes, a new node is being created
	 */
	if (method != 0)
		name[namelen++] = method;
	else
		name[namelen++] = CTL_CREATE;

	sz = sizeof(node);
	rc = prog_sysctl(&name[0], namelen, &node, &sz, &node, sizeof(node));

	if (rc == -1) {
		sysctlperror("%s: CTL_CREATE failed: %s\n",
			     nname, strerror(errno));
		EXIT(EXIT_FAILURE);
	}
	else {
		if (!qflag && !nflag)
			printf("%s(%s): (created)\n", nname, st(type));
		stale = 1;
	}
}

static void
parse_destroy(char *l)
{
	struct sysctlnode node;
	size_t sz;
	int name[CTL_MAXNAME], rc;
	u_int namelen;

	if (!wflag) {
		sysctlperror("Must specify -w to destroy nodes\n");
		exit(EXIT_FAILURE);
	}

	memset(name, 0, sizeof(name));
	namelen = sizeof(name) / sizeof(name[0]);
	sz = sizeof(gsname);
	rc = sysctlgetmibinfo(l, &name[0], &namelen, gsname, &sz, NULL,
			      SYSCTL_VERSION);
	if (rc == -1) {
		sysctlparseerror(namelen, l);
		EXIT(EXIT_FAILURE);
	}

	memset(&node, 0, sizeof(node));
	node.sysctl_flags = SYSCTL_VERSION;
	node.sysctl_num = name[namelen - 1];
	name[namelen - 1] = CTL_DESTROY;

	sz = sizeof(node);
	rc = prog_sysctl(&name[0], namelen, &node, &sz, &node, sizeof(node));

	if (rc == -1) {
		sysctlperror("%s: CTL_DESTROY failed: %s\n",
			     l, strerror(errno));
		EXIT(EXIT_FAILURE);
	}
	else {
		if (!qflag && !nflag)
			printf("%s(%s): (destroyed)\n", gsname,
			       st(SYSCTL_TYPE(node.sysctl_flags)));
		stale = 1;
	}
}

static void
parse_describe(char *l)
{
	struct sysctlnode newdesc;
	char buf[1024], *value;
	struct sysctldesc *d = (void*)&buf[0];
	int name[CTL_MAXNAME], rc;
	u_int namelen;
	size_t sz;

	if (!wflag) {
		sysctlperror("Must specify -w to set descriptions\n");
		exit(EXIT_FAILURE);
	}

	value = strchr(l, '=');
	*value++ = '\0';

	memset(name, 0, sizeof(name));
	namelen = sizeof(name) / sizeof(name[0]);
	sz = sizeof(gsname);
	rc = sysctlgetmibinfo(l, &name[0], &namelen, gsname, &sz, NULL,
			      SYSCTL_VERSION);
	if (rc == -1) {
		sysctlparseerror(namelen, l);
		EXIT(EXIT_FAILURE);
	}

	sz = sizeof(buf);
	memset(&newdesc, 0, sizeof(newdesc));
	newdesc.sysctl_flags = SYSCTL_VERSION|CTLFLAG_OWNDESC;
	newdesc.sysctl_num = name[namelen - 1];
	newdesc.sysctl_desc = value;
	name[namelen - 1] = CTL_DESCRIBE;
	rc = prog_sysctl(name, namelen, d, &sz, &newdesc, sizeof(newdesc));
	if (rc == -1)
		sysctlperror("%s: CTL_DESCRIBE failed: %s\n",
			     gsname, strerror(errno));
	else if (d->descr_len == 1)
		sysctlperror("%s: description not set\n", gsname);
	else if (!qflag && !nflag)
		printf("%s: %s\n", gsname, d->descr_str);
}

/*
 * ********************************************************************
 * when things go wrong...
 * ********************************************************************
 */
static void
usage(void)
{
	const char *progname = getprogname();

	(void)fprintf(stderr,
		      "usage:\t%s %s\n"
		      "\t%s %s\n"
		      "\t%s %s\n"
		      "\t%s %s\n"
		      "\t%s %s\n"
		      "\t%s %s\n",
		      progname, "[-dneq] [-x[x]|-r] variable ...",
		      progname, "[-ne] [-q] -w variable=value ...",
		      progname, "[-dne] -a",
		      progname, "[-dne] -A",
		      progname, "[-ne] -M",
		      progname, "[-dne] [-q] -f file");
	exit(EXIT_FAILURE);
}

static void
getdesc1(int *name, u_int namelen, struct sysctlnode *pnode)
{
	struct sysctlnode node;
	char buf[1024], *desc;
	struct sysctldesc *d = (void*)buf;
	size_t sz = sizeof(buf);
	int rc;

	memset(&node, 0, sizeof(node));
	node.sysctl_flags = SYSCTL_VERSION;
	node.sysctl_num = name[namelen - 1];
	name[namelen - 1] = CTL_DESCRIBE;
	rc = prog_sysctl(name, namelen, d, &sz, &node, sizeof(node));

	if (rc == -1 ||
	    d->descr_len == 1 ||
	    d->descr_num != pnode->sysctl_num ||
	    d->descr_ver != pnode->sysctl_ver)
		desc = (char *)-1;
	else
		desc = malloc(d->descr_len);

	if (desc == NULL)
		desc = (char *)-1;
	if (desc != (char *)-1)
		memcpy(desc, &d->descr_str[0], d->descr_len);
	name[namelen - 1] = node.sysctl_num;
	if (pnode->sysctl_desc != NULL &&
	    pnode->sysctl_desc != (const char *)-1)
		free(__UNCONST(pnode->sysctl_desc));
	pnode->sysctl_desc = desc;
}

static void
getdesc(int *name, u_int namelen, struct sysctlnode *pnode)
{
	struct sysctlnode *node = pnode->sysctl_child;
	struct sysctldesc *d, *p, *plim;
	char *desc;
	size_t i, sz, child_cnt;
	int rc;

	sz = 128 * pnode->sysctl_clen;
	name[namelen] = CTL_DESCRIBE;

	/*
	 * attempt *twice* to get the description chunk.  if two tries
	 * doesn't work, give up.
	 */
	i = 0;
	do {
		d = malloc(sz);
		if (d == NULL)
			return;
		rc = prog_sysctl(name, namelen + 1, d, &sz, NULL, 0);
		if (rc == -1) {
			free(d);
			d = NULL;
			if (i == 0 && errno == ENOMEM)
				i = 1;
			else
				return;
		}
	} while (d == NULL);

	/*
	 * hokey nested loop here, giving O(n**2) behavior, but should
	 * suffice for now
	 */
	plim = /*LINTED ptr cast*/(struct sysctldesc *)((char*)d + sz);
	child_cnt = (pnode->sysctl_flags & CTLTYPE_NODE) ? pnode->sysctl_clen
	    : 0;
	for (i = 0; i < child_cnt; i++) {
		node = &pnode->sysctl_child[i];
		for (p = d; p < plim; p = NEXT_DESCR(p))
			if (node->sysctl_num == p->descr_num)
				break;
		if (p < plim && node->sysctl_ver == p->descr_ver) {
			/*
			 * match found, attempt to attach description
			 */
			if (p->descr_len == 1)
				desc = NULL;
			else
				desc = malloc(p->descr_len);
			if (desc == NULL)
				desc = (char *)-1;
			else
				memcpy(desc, &p->descr_str[0], p->descr_len);
			node->sysctl_desc = desc;
		}
	}

	free(d);
}

static void
trim_whitespace(char *s, int dir)
{
	char *i, *o;

	i = o = s;
	if (dir & 1)
		while (isspace((unsigned char)*i))
			i++;
	while ((*o++ = *i++) != '\0');
	o -= 2; /* already past nul, skip back to before it */
	if (dir & 2)
		while (o > s && isspace((unsigned char)*o))
			*o-- = '\0';
}

void
sysctlerror(int soft)
{
	if (soft) {
		switch (errno) {
		case ENOENT:
		case ENOPROTOOPT:
		case ENOTDIR:
		case EINVAL:
		case EOPNOTSUPP:
		case EPROTONOSUPPORT:
			if (Aflag || req)
				sysctlperror("%s: the value is not available "
				    "(%s)\n", gsname, strerror(errno));
			return;
		}
	}

	if (Aflag || req)
		sysctlperror("%s: %s\n", gsname, strerror(errno));
	if (!soft)
		EXIT(EXIT_FAILURE);
}

void
sysctlparseerror(u_int namelen, const char *pname)
{

	if (qflag) {
		errs++;
		return;
	}
	sysctlperror("%s level name '%s' in '%s' is invalid\n",
		     lname[namelen], gsname, pname);
}

static void
sysctlperror(const char *fmt, ...)
{
	va_list ap;

	(void)fprintf(warnfp, "%s: ", getprogname());
	if (fn)
		(void)fprintf(warnfp, "%s#%zu: ", fn, nr);
	va_start(ap, fmt);
	(void)vfprintf(warnfp, fmt, ap);
	va_end(ap);
	errs++;
}


/*
 * ********************************************************************
 * how to write to a "simple" node
 * ********************************************************************
 */
static void
write_number(int *name, u_int namelen, struct sysctlnode *node, char *value)
{
	u_int ii, io;
	u_quad_t qi, qo;
	size_t si, so;
	bool bi, bo;
	int rc;
	void *i, *o;
	char *t;

	if (fn)
		trim_whitespace(value, 3);

	si = so = 0;
	i = o = NULL;
	bi = bo = false;
	errno = 0;
	qi = strtouq(value, &t, 0);
	if (qi == UQUAD_MAX && errno == ERANGE) {
		sysctlperror("%s: %s\n", value, strerror(errno));
		EXIT(EXIT_FAILURE);
	}
	if (t == value || *t != '\0') {
		sysctlperror("%s: not a number\n", value);
		EXIT(EXIT_FAILURE);
	}

	switch (SYSCTL_TYPE(node->sysctl_flags)) {
	case CTLTYPE_INT:
		ii = (u_int)qi;
		io = (u_int)(qi >> 32);
		if (io != (u_int)-1 && io != 0) {
			sysctlperror("%s: %s\n", value, strerror(ERANGE));
			EXIT(EXIT_FAILURE);
		}
		o = &io;
		so = sizeof(io);
		i = &ii;
		si = sizeof(ii);
		break;
	case CTLTYPE_BOOL:
		bi = (bool)qi;
		o = &bo;
		so = sizeof(bo);
		i = &bi;
		si = sizeof(bi);
		break;
	case CTLTYPE_QUAD:
		o = &qo;
		so = sizeof(qo);
		i = &qi;
		si = sizeof(qi);
		break;
	}

	rc = prog_sysctl(name, namelen, o, &so, i, si);
	if (rc == -1) {
		sysctlerror(0);
		return;
	}

	switch (SYSCTL_TYPE(node->sysctl_flags)) {
	case CTLTYPE_INT:
		display_number(node, gsname, &io, sizeof(io), DISPLAY_OLD);
		display_number(node, gsname, &ii, sizeof(ii), DISPLAY_NEW);
		break;
	case CTLTYPE_BOOL:
		display_number(node, gsname, &bo, sizeof(bo), DISPLAY_OLD);
		display_number(node, gsname, &bi, sizeof(bi), DISPLAY_NEW);
		break;
	case CTLTYPE_QUAD:
		display_number(node, gsname, &qo, sizeof(qo), DISPLAY_OLD);
		display_number(node, gsname, &qi, sizeof(qi), DISPLAY_NEW);
		break;
	}
}

static void
write_string(int *name, u_int namelen, struct sysctlnode *node, char *value)
{
	char *i, *o;
	size_t si, so;
	int rc;

	i = value;
	si = strlen(i) + 1;
	so = node->sysctl_size;
	if (si > so && so != 0) {
		sysctlperror("%s: string too long\n", value);
		EXIT(EXIT_FAILURE);
	}
	o = malloc(so);
	if (o == NULL) {
		sysctlperror("%s: !malloc failed!\n", gsname);
		exit(EXIT_FAILURE);
	}

	rc = prog_sysctl(name, namelen, o, &so, i, si);
	if (rc == -1) {
		sysctlerror(0);
		return;
	}

	display_string(node, gsname, o, so, DISPLAY_OLD);
	display_string(node, gsname, i, si, DISPLAY_NEW);
	free(o);
}

/*
 * ********************************************************************
 * simple ways to print stuff consistently
 * ********************************************************************
 */
static void
display_number(const struct sysctlnode *node, const char *name,
	       const void *data, size_t sz, int n)
{
	u_quad_t q;
	bool b;
	int i;

	if (qflag)
		return;
	if ((nflag || rflag) && (n == DISPLAY_OLD))
		return;

	if (rflag && n != DISPLAY_OLD) {
		fwrite(data, sz, 1, stdout);
		return;
	}

	if (!nflag) {
		if (n == DISPLAY_VALUE)
			printf("%s%s", name, eq);
		else if (n == DISPLAY_OLD)
			printf("%s: ", name);
	}

	if (xflag > 1) {
		if (n != DISPLAY_NEW)
			printf("\n");
		hex_dump(data, sz);
		return;
	}

	switch (SYSCTL_TYPE(node->sysctl_flags)) {
	case CTLTYPE_INT:
		memcpy(&i, data, sz);
		if (xflag)
			printf("0x%0*x", (int)sz * 2, i);
		else if (node->sysctl_flags & CTLFLAG_HEX)
			printf("%#x", i);
		else if (node->sysctl_flags & CTLFLAG_UNSIGNED)
			printf("%u", i);
		else
			printf("%d", i);
		break;
	case CTLTYPE_BOOL:
		memcpy(&b, data, sz);
		if (xflag)
			printf("0x%0*x", (int)sz * 2, b);
		else if (node->sysctl_flags & CTLFLAG_HEX)
			printf("%#x", b);
		else
			printf("%d", b);
		break;
	case CTLTYPE_QUAD:
		memcpy(&q, data, sz);
		if (xflag)
			printf("0x%0*" PRIx64, (int)sz * 2, q);
		else if (node->sysctl_flags & CTLFLAG_HEX)
			printf("%#" PRIx64, q);
		else if (node->sysctl_flags & CTLFLAG_UNSIGNED)
			printf("%" PRIu64, q);
		else
			printf("%" PRIu64, q);
		break;
	}

	if (n == DISPLAY_OLD)
		printf(" -> ");
	else
		printf("\n");
}

static void
display_string(const struct sysctlnode *node, const char *name,
	       const void *data, size_t sz, int n)
{
	const unsigned char *buf = data;
	int ni;

	if (qflag)
		return;
	if ((nflag || rflag) && (n == DISPLAY_OLD))
		return;

	if (rflag && n != DISPLAY_OLD) {
		fwrite(data, sz, 1, stdout);
		return;
	}

	if (!nflag) {
		if (n == DISPLAY_VALUE)
			printf("%s%s", name, eq);
		else if (n == DISPLAY_OLD)
			printf("%s: ", name);
	}

	if (xflag > 1) {
		if (n != DISPLAY_NEW)
			printf("\n");
		hex_dump(data, sz);
		return;
	}

	if (xflag || node->sysctl_flags & CTLFLAG_HEX) {
		for (ni = 0; ni < (int)sz; ni++) {
			if (xflag)
				printf("%02x", buf[ni]);
			if (buf[ni] == '\0')
				break;
			if (!xflag)
				printf("\\x%2.2x", buf[ni]);
		}
	}
	else
		printf("%.*s", (int)sz, buf);

	if (n == DISPLAY_OLD)
		printf(" -> ");
	else
		printf("\n");
}

/*ARGSUSED*/
static void
display_struct(const struct sysctlnode *node, const char *name,
	       const void *data, size_t sz, int n)
{
	const unsigned char *buf = data;
	int ni;
	size_t more;

	if (qflag)
		return;
	if (!(xflag || rflag)) {
		if (Aflag || req)
			sysctlperror(
			    "%s: this type is unknown to this program\n",
			    gsname);
		return;
	}
	if ((nflag || rflag) && (n == DISPLAY_OLD))
		return;

	if (rflag && n != DISPLAY_OLD) {
		fwrite(data, sz, 1, stdout);
		return;
	}

        if (!nflag) {
                if (n == DISPLAY_VALUE)
                        printf("%s%s", name, eq);
                else if (n == DISPLAY_OLD)
                        printf("%s: ", name);
        }

	if (xflag > 1) {
		if (n != DISPLAY_NEW)
			printf("\n");
		hex_dump(data, sz);
		return;
	}

	if (sz > 16) {
		more = sz - 16;
		sz = 16;
	}
	else
		more = 0;
	for (ni = 0; ni < (int)sz; ni++)
		printf("%02x", buf[ni]);
	if (more)
		printf("...(%zu more bytes)", more);
	printf("\n");
}

static void
hex_dump(const unsigned char *buf, size_t len)
{
	unsigned int i;
	int j;
	char line[80], tmp[12];

	memset(line, ' ', sizeof(line));
	for (i = 0, j = 15; i < len; i++) {
		j = i % 16;
		/* reset line */
		if (j == 0) {
			line[58] = '|';
			line[77] = '|';
			line[78] = 0;
			snprintf(tmp, sizeof(tmp), "%07x", i);
			memcpy(&line[0], tmp, 7);
		}
		/* copy out hex version of byte */
		snprintf(tmp, sizeof(tmp), "%02x", buf[i]);
		memcpy(&line[9 + j * 3], tmp, 2);
		/* copy out plain version of byte */
		line[60 + j] = (isprint(buf[i])) ? buf[i] : '.';
		/* print a full line and erase it */
		if (j == 15) {
			printf("%s\n", line);
			memset(line, ' ', sizeof(line));
		}
	}
	if (line[0] != ' ')
		printf("%s\n", line);
	printf("%07zu bytes\n", len);
}

/*
 * ********************************************************************
 * functions that handle particular nodes
 * ********************************************************************
 */
/*ARGSUSED*/
static void
printother(HANDLER_ARGS)
{
	int rc;
	void *p;
	size_t sz1, sz2;

	if (!(Aflag || req) || Mflag)
		return;

	/*
	 * okay...you asked for it, so let's give it a go
	 */
	while (type != CTLTYPE_NODE && (xflag || rflag)) {
		rc = prog_sysctl(name, namelen, NULL, &sz1, NULL, 0);
		if (rc == -1 || sz1 == 0)
			break;
		p = malloc(sz1);
		if (p == NULL)
			break;
		sz2 = sz1;
		rc = prog_sysctl(name, namelen, p, &sz2, NULL, 0);
		if (rc == -1 || sz1 != sz2) {
			free(p);
			break;
		}
		display_struct(pnode, gsname, p, sz1, DISPLAY_VALUE);
		free(p);
		return;
	}

	/*
	 * that didn't work...do we have a specific message for this
	 * thing?
	 */
	if (v != NULL) {
		sysctlperror("%s: use '%s' to view this information\n",
			     gsname, (const char *)v);
		return;
	}

	/*
	 * hmm...i wonder if we have any generic hints?
	 */
	switch (name[0]) {
	case CTL_NET:
		sysctlperror("%s: use 'netstat' to view this information\n",
			     sname);
		break;
	case CTL_DEBUG:
		sysctlperror("%s: missing 'options DEBUG' from kernel?\n",
			     sname);
		break;
	case CTL_DDB:
		sysctlperror("%s: missing 'options DDB' from kernel?\n",
			     sname);
		break;
	case CTL_VENDOR:
		sysctlperror("%s: no vendor extensions installed\n",
			     sname);
		break;
	}
}

/*ARGSUSED*/
static void
kern_clockrate(HANDLER_ARGS)
{
	struct clockinfo clkinfo;
	size_t sz;
	int rc;

	sz = sizeof(clkinfo);
	rc = prog_sysctl(name, namelen, &clkinfo, &sz, NULL, 0);
	if (rc == -1) {
		sysctlerror(1);
		return;
	}
	if (sz != sizeof(clkinfo))
		errx(EXIT_FAILURE, "%s: !returned size wrong!", sname);

	if (xflag || rflag) {
		display_struct(pnode, sname, &clkinfo, sz,
			       DISPLAY_VALUE);
		return;
	}
	else if (!nflag)
		printf("%s: ", sname);
	printf("tick = %d, tickadj = %d, hz = %d, profhz = %d, stathz = %d\n",
	       clkinfo.tick, clkinfo.tickadj,
	       clkinfo.hz, clkinfo.profhz, clkinfo.stathz);
}

/*ARGSUSED*/
static void
kern_boottime(HANDLER_ARGS)
{
	struct timeval timeval;
	time_t boottime;
	size_t sz;
	int rc;

	sz = sizeof(timeval);
	rc = prog_sysctl(name, namelen, &timeval, &sz, NULL, 0);
	if (rc == -1) {
		sysctlerror(1);
		return;
	}
	if (sz != sizeof(timeval))
		errx(EXIT_FAILURE, "%s: !returned size wrong!", sname);

	boottime = timeval.tv_sec;
	if (xflag || rflag)
		display_struct(pnode, sname, &timeval, sz,
			       DISPLAY_VALUE);
	else if (!nflag)
		/* ctime() provides the \n */
		printf("%s%s%s", sname, eq, ctime(&boottime));
	else if (nflag == 1)
		printf("%ld\n", (long)boottime);
	else
		printf("%ld.%06ld\n", (long)timeval.tv_sec,
		       (long)timeval.tv_usec);
}

/*ARGSUSED*/
static void
kern_consdev(HANDLER_ARGS)
{
	dev_t cons;
	size_t sz;
	int rc;

	sz = sizeof(cons);
	rc = prog_sysctl(name, namelen, &cons, &sz, NULL, 0);
	if (rc == -1) {
		sysctlerror(1);
		return;
	}
	if (sz != sizeof(cons))
		errx(EXIT_FAILURE, "%s: !returned size wrong!", sname);

	if (xflag || rflag)
		display_struct(pnode, sname, &cons, sz,
			       DISPLAY_VALUE);
	else {
		if (!nflag)
			printf("%s%s", sname, eq);
		if (nflag < 2 && (sname = devname(cons, S_IFCHR)) != NULL)
			printf("%s\n", sname);
		else
			printf("0x%llx\n", (unsigned long long)cons);
	}
}

/*ARGSUSED*/
static void
kern_cp_time(HANDLER_ARGS)
{
	u_int64_t *cp_time;
	size_t sz, osz;
	int rc, i, n;
	char s[sizeof("kern.cp_time.nnnnnn")];
	const char *tname;

	/*
	 * three things to do here.
	 * case 1: get sum (no Aflag and namelen == 2)
	 * case 2: get specific processor (namelen == 3)
	 * case 3: get all processors (Aflag and namelen == 2)
	 */

	if (namelen == 2 && Aflag) {
		sz = sizeof(n);
		rc = sysctlbyname("hw.ncpu", &n, &sz, NULL, 0);
		if (rc != 0)
			return; /* XXX print an error, eh? */
		n++; /* Add on space for the sum. */
		sz = n * sizeof(u_int64_t) * CPUSTATES;
	}
	else {
		n = -1; /* Just print one data set. */
		sz = sizeof(u_int64_t) * CPUSTATES;
	}

	cp_time = malloc(sz);
	if (cp_time == NULL) {
		sysctlerror(1);
		return;
	}

	osz = sz;
	rc = prog_sysctl(name, namelen, cp_time + (n != -1) * CPUSTATES, &osz,
		    NULL, 0);

	if (rc == -1) {
		sysctlerror(1);
		free(cp_time);
		return;
	}

	/*
	 * Check, but account for space we'll occupy with the sum.
	 */
	if (osz != sz - (n != -1) * CPUSTATES * sizeof(u_int64_t))
		errx(EXIT_FAILURE, "%s: !returned size wrong!", sname);

	/*
	 * Compute the actual sum.  Two calls would be easier (we
	 * could just call ourselves recursively above), but the
	 * numbers wouldn't add up.
	 */
	if (n != -1) {
		memset(cp_time, 0, sizeof(u_int64_t) * CPUSTATES);
		for (i = 1; i < n; i++) {
			cp_time[CP_USER] += cp_time[i * CPUSTATES + CP_USER];
                        cp_time[CP_NICE] += cp_time[i * CPUSTATES + CP_NICE];
                        cp_time[CP_SYS] += cp_time[i * CPUSTATES + CP_SYS];
                        cp_time[CP_INTR] += cp_time[i * CPUSTATES + CP_INTR];
                        cp_time[CP_IDLE] += cp_time[i * CPUSTATES + CP_IDLE];
		}
	}

	tname = sname;
	for (i = 0; n == -1 || i < n; i++) {
		if (i > 0) {
			(void)snprintf(s, sizeof(s), "%s%s%d", sname, sep,
				       i - 1);
			tname = s;
		}
		if (xflag || rflag)
			display_struct(pnode, tname, cp_time + (i * CPUSTATES),
				       sizeof(u_int64_t) * CPUSTATES,
				       DISPLAY_VALUE);
		else {
			if (!nflag)
				printf("%s: ", tname);
			printf("user = %" PRIu64
			       ", nice = %" PRIu64
			       ", sys = %" PRIu64
			       ", intr = %" PRIu64
			       ", idle = %" PRIu64
			       "\n",
			       cp_time[i * CPUSTATES + CP_USER],
			       cp_time[i * CPUSTATES + CP_NICE],
			       cp_time[i * CPUSTATES + CP_SYS],
			       cp_time[i * CPUSTATES + CP_INTR],
			       cp_time[i * CPUSTATES + CP_IDLE]);
		}
		/*
		 * Just printing the one node.
		 */
		if (n == -1)
			break;
	}

	free(cp_time);
}

/*ARGSUSED*/
static void
kern_drivers(HANDLER_ARGS)
{
	struct kinfo_drivers *kd;
	size_t sz, i;
	int rc;
	const char *comma;

	rc = prog_sysctl(name, namelen, NULL, &sz, NULL, 0);
	if (rc == -1) {
		sysctlerror(1);
		return;
	}

	if (sz % sizeof(*kd))
		err(EXIT_FAILURE, "bad size %zu for kern.drivers", sz);

	kd = malloc(sz);
	if (kd == NULL) {
		sysctlerror(1);
		return;
	}

	rc = prog_sysctl(name, namelen, kd, &sz, NULL, 0);
	if (rc == -1) {
		sysctlerror(1);
		free(kd);
		return;
	}

	comma = "";
	if (!nflag)
		printf("%s%s", sname, eq);
	for (i = 0, sz /= sizeof(*kd); i < sz; i++) {
		(void)printf("%s[%d %d %s]", comma, kd[i].d_cmajor,
		    kd[i].d_bmajor, kd[i].d_name);
		comma = ", ";
	}
	(void)printf("\n");
	free(kd);
}

/*ARGSUSED*/
static void
kern_cp_id(HANDLER_ARGS)
{
	u_int64_t *cp_id;
	size_t sz, osz;
	int rc, i, n;
	char s[sizeof("kern.cp_id.nnnnnn")];
	const char *tname;
	struct sysctlnode node = *pnode;

	/*
	 * three things to do here.
	 * case 1: print a specific cpu id (namelen == 3)
	 * case 2: print all cpu ids separately (Aflag set)
	 * case 3: print all cpu ids on one line
	 */

	if (namelen == 2) {
		sz = sizeof(n);
		rc = sysctlbyname("hw.ncpu", &n, &sz, NULL, 0);
		if (rc != 0)
			return; /* XXX print an error, eh? */
		sz = n * sizeof(u_int64_t);
	}
	else {
		n = -1; /* Just print one cpu id. */
		sz = sizeof(u_int64_t);
	}

	cp_id = malloc(sz);
	if (cp_id == NULL) {
		sysctlerror(1);
		return;
	}

	osz = sz;
	rc = prog_sysctl(name, namelen, cp_id, &osz, NULL, 0);
	if (rc == -1) {
		sysctlerror(1);
		free(cp_id);
		return;
	}

	/*
	 * Check that we got back what we asked for.
	 */
	if (osz != sz)
		errx(EXIT_FAILURE, "%s: !returned size wrong!", sname);

	/* pretend for output purposes */
	node.sysctl_flags = SYSCTL_FLAGS(pnode->sysctl_flags) |
		SYSCTL_TYPE(CTLTYPE_QUAD);

	tname = sname;
	if (namelen == 3)
		display_number(&node, tname, cp_id,
			       sizeof(u_int64_t),
			       DISPLAY_VALUE);
	else if (Aflag) {
		for (i = 0; i < n; i++)
			(void)snprintf(s, sizeof(s), "%s%s%d", sname, sep, i);
			tname = s;
			display_number(&node, tname, &cp_id[i],
				       sizeof(u_int64_t),
				       DISPLAY_VALUE);
	}
	else {
		if (xflag || rflag)
			display_struct(pnode, tname, cp_id, sz, DISPLAY_VALUE);
		else {
			if (!nflag)
				printf("%s: ", tname);
			for (i = 0; i < n; i++) {
				if (i)
					printf(", ");
				printf("%d = %" PRIu64, i, cp_id[i]);
			}
			printf("\n");
		}
	}

	free(cp_id);
}

/*ARGSUSED*/
static void
vm_loadavg(HANDLER_ARGS)
{
	struct loadavg loadavg;
	size_t sz;
	int rc;

	sz = sizeof(loadavg);
	rc = prog_sysctl(name, namelen, &loadavg, &sz, NULL, 0);
	if (rc == -1) {
		sysctlerror(1);
		return;
	}
	if (sz != sizeof(loadavg))
		errx(EXIT_FAILURE, "%s: !returned size wrong!", sname);

	if (xflag || rflag) {
		display_struct(pnode, sname, &loadavg, sz,
			       DISPLAY_VALUE);
		return;
	}
	if (!nflag)
		printf("%s: ", sname);
	printf("%.2f %.2f %.2f\n",
	       (double) loadavg.ldavg[0] / loadavg.fscale,
	       (double) loadavg.ldavg[1] / loadavg.fscale,
	       (double) loadavg.ldavg[2] / loadavg.fscale);
}

/*ARGSUSED*/
static void
proc_limit(HANDLER_ARGS)
{
	u_quad_t olim, *newp, nlim;
	size_t osz, nsz;
	char *t;
	int rc;

	if (fn)
		trim_whitespace(value, 3);

	osz = sizeof(olim);
	if (value != NULL) {
		nsz = sizeof(nlim);
		newp = &nlim;
		if (strcmp(value, "unlimited") == 0)
			nlim = RLIM_INFINITY;
		else {
			errno = 0;
			nlim = strtouq(value, &t, 0);
			if (t == value || *t != '\0' || errno != 0) {
				sysctlperror("%s: '%s' is not a valid limit\n",
					     sname, value);
				EXIT(EXIT_FAILURE);
			}
		}
	}
	else {
		nsz = 0;
		newp = NULL;
	}

	rc = prog_sysctl(name, namelen, &olim, &osz, newp, nsz);
	if (rc == -1) {
		sysctlerror(newp == NULL);
		return;
	}

	if (newp && qflag)
		return;

	if (rflag || xflag || olim != RLIM_INFINITY)
		display_number(pnode, sname, &olim, sizeof(olim),
			       newp ? DISPLAY_OLD : DISPLAY_VALUE);
	else
		display_string(pnode, sname, "unlimited", 10,
			       newp ? DISPLAY_OLD : DISPLAY_VALUE);

	if (newp) {
		if (rflag || xflag || nlim != RLIM_INFINITY)
			display_number(pnode, sname, &nlim, sizeof(nlim),
				       DISPLAY_NEW);
		else
			display_string(pnode, sname, "unlimited", 10,
				       DISPLAY_NEW);
	}
}

#ifdef CPU_DISKINFO
/*ARGSUSED*/
static void
machdep_diskinfo(HANDLER_ARGS)
{
	struct disklist *dl;
	struct biosdisk_info *bi;
	struct nativedisk_info *ni;
	int rc;
	size_t sz;
	uint i, b, lim;

	rc = prog_sysctl(name, namelen, NULL, &sz, NULL, 0);
	if (rc == -1) {
		sysctlerror(1);
		return;
	}
	dl = malloc(sz);
	if (dl == NULL) {
		sysctlerror(1);
		return;
	}
	rc = prog_sysctl(name, namelen, dl, &sz, NULL, 0);
	if (rc == -1) {
		sysctlerror(1);
		return;
	}

	if (!nflag)
		printf("%s: ", sname);
	lim = dl->dl_nbiosdisks;
	if (lim > MAX_BIOSDISKS)
		lim = MAX_BIOSDISKS;
	for (bi = dl->dl_biosdisks, i = 0; i < lim; bi++, i++)
		printf("%x:%" PRIu64 "(%d/%d/%d),%x ",
		       bi->bi_dev, bi->bi_lbasecs,
		       bi->bi_cyl, bi->bi_head, bi->bi_sec,
		       bi->bi_flags);
	lim = dl->dl_nnativedisks;
	ni = dl->dl_nativedisks;
	bi = dl->dl_biosdisks;
	/* LINTED -- pointer casts are tedious */
	if ((char *)&ni[lim] != (char *)dl + sz) {
		sysctlperror("%s: size mismatch\n", gsname);
		return;
	}
	for (i = 0; i < lim; ni++, i++) {
		char t = ':';
		printf(" %.*s", (int)sizeof ni->ni_devname,
		       ni->ni_devname);
		for (b = 0; b < (unsigned int)ni->ni_nmatches; t = ',', b++)
			printf("%c%x", t,
			       bi[ni->ni_biosmatches[b]].bi_dev);
	}
	printf("\n");
	free(dl);
}
#endif /* CPU_DISKINFO */

/*ARGSUSED*/
static void
mode_bits(HANDLER_ARGS)
{
	char buf[12], outbuf[100];
	int o, m, *newp, rc;
	size_t osz, nsz;
	mode_t om, mm;

	if (fn)
		trim_whitespace(value, 3);

	newp = NULL;
	osz = sizeof(o);
	if (value != NULL) {
		void *foo;
		int tt;
		size_t ttsz = sizeof(tt);
		mode_t old_umask;

		nsz = sizeof(m);
		newp = &m;
		errno = 0;
		rc = prog_sysctl(name, namelen, &tt, &ttsz, NULL, 0);
		if (rc == -1) {
			sysctlperror("%s: failed query\n", sname);
			return;
		}

		old_umask = umask(0);
		foo = setmode(value);
		umask(old_umask);
		if (foo == NULL) {
			sysctlperror("%s: '%s' is an invalid mode\n", sname,
				     value);
			EXIT(EXIT_FAILURE);
		}
		old_umask = umask(0);
		m = getmode(foo, (mode_t)tt);
		umask(old_umask);
		if (errno) {
			sysctlperror("%s: '%s' is an invalid mode\n", sname,
				     value);
			EXIT(EXIT_FAILURE);
		}
	}
	else {
		nsz = 0;
		newp = NULL;
	}

	rc = prog_sysctl(name, namelen, &o, &osz, newp, nsz);
	if (rc == -1) {
		sysctlerror(newp == NULL);
		return;
	}

	if (newp && qflag)
		return;

	om = (mode_t)o;
	mm = (mode_t)m;

	if (rflag || xflag)
		display_number(pnode, sname, &o, sizeof(o),
			       newp ? DISPLAY_OLD : DISPLAY_VALUE);
	else {
		memset(buf, 0, sizeof(buf));
		strmode(om, buf);
		rc = snprintf(outbuf, sizeof(outbuf), "%04o (%s)", om, buf + 1);
		display_string(pnode, sname, outbuf, rc, newp ? DISPLAY_OLD : DISPLAY_VALUE);
	}

	if (newp) {
		if (rflag || xflag)
			display_number(pnode, sname, &m, sizeof(m),
				       DISPLAY_NEW);
		else {
			memset(buf, 0, sizeof(buf));
			strmode(mm, buf);
			rc = snprintf(outbuf, sizeof(outbuf), "%04o (%s)", mm, buf + 1);
			display_string(pnode, sname, outbuf, rc, DISPLAY_NEW);
		}
	}
}

typedef __BITMAP_TYPE(, uint32_t, 0x10000) bitmap;

static char *
bitmask_print(const bitmap *o)
{
	char *s, *os;

	s = os = NULL;
	for (size_t i = 0; i < MAXPORTS; i++)
		if (__BITMAP_ISSET(i, o)) {
			int rv;

			if (os)
			    	rv = asprintf(&s, "%s,%zu", os, i);
			else
			    	rv = asprintf(&s, "%zu", i);
			if (rv == -1)
				err(EXIT_FAILURE, "%s 1", __func__);
			free(os);
			os = s;
		}
	if (s == NULL && (s = strdup("")) == NULL)
		err(EXIT_FAILURE, "%s 2", __func__);
	return s;
}

static void
bitmask_scan(const void *v, bitmap *o)
{
	char *s = strdup(v);
	if (s == NULL)
		err(EXIT_FAILURE, "%s", __func__);

	__BITMAP_ZERO(o);
	for (s = strtok(s, ","); s; s = strtok(NULL, ",")) {
		char *e;
		errno = 0;
		unsigned long l = strtoul(s, &e, 0);
		if ((l == ULONG_MAX && errno == ERANGE) || s == e || *e)
			errx(EXIT_FAILURE, "Invalid port: %s", s);
		if (l >= MAXPORTS)
			errx(EXIT_FAILURE, "Port out of range: %s", s);
		__BITMAP_SET(l, o);
	}
}


static void
reserve(HANDLER_ARGS)
{
	int rc;
	size_t osz, nsz;
	bitmap o, n;

	if (fn)
		trim_whitespace(value, 3);

	osz = sizeof(o);
	if (value) {
		bitmask_scan(value, &n);
		value = (char *)&n;
		nsz = sizeof(n);
	} else
		nsz = 0;

	rc = prog_sysctl(name, namelen, &o, &osz, value, nsz);
	if (rc == -1) {
		sysctlerror(value == NULL);
		return;
	}

	if (value && qflag)
		return;

	if (rflag || xflag)
		display_struct(pnode, sname, &o, sizeof(o),
		    value ? DISPLAY_OLD : DISPLAY_VALUE);
	else {
		char *s = bitmask_print(&o);
		display_string(pnode, sname, s, strlen(s),
		    value ? DISPLAY_OLD : DISPLAY_VALUE);
		free(s);
	}

	if (value) {
		if (rflag || xflag)
			display_struct(pnode, sname, &n, sizeof(n),
			    DISPLAY_NEW);
		else {
			char *s = bitmask_print(&n);
			display_string(pnode, sname, s, strlen(s), DISPLAY_NEW);
			free(s);
		}
	}
}
