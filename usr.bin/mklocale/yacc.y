/*	$NetBSD: yacc.y,v 1.31 2010/06/13 04:14:57 tnozaki Exp $	*/

%{
/*-
 * Copyright (c) 1993
 *	The Regents of the University of California.  All rights reserved.
 *
 * This code is derived from software contributed to Berkeley by
 * Paul Borman at Krystal Technologies.
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

#if HAVE_NBTOOL_CONFIG_H
#include "nbtool_config.h"
#endif

#include <sys/cdefs.h>
#ifndef lint
#if 0
static char sccsid[] = "@(#)yacc.y	8.1 (Berkeley) 6/6/93";
static char rcsid[] = "$FreeBSD$";
#else
__RCSID("$NetBSD: yacc.y,v 1.31 2010/06/13 04:14:57 tnozaki Exp $");
#endif
#endif /* not lint */

#include <sys/types.h>
#include <netinet/in.h>	/* Needed by <arpa/inet.h> on NetBSD 1.5. */
#include <arpa/inet.h>	/* Needed for htonl on POSIX systems. */

#include <err.h>
#include <locale.h>
#include <stddef.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <ctype.h>

#include "runetype_file.h"

#include "ldef.h"

const char	*locale_file = "<stdout>";

rune_map	maplower = { { 0, }, };
rune_map	mapupper = { { 0, }, };
rune_map	types = { { 0, }, };

_FileRuneLocale new_locale = { { 0, }, };

size_t rl_variable_len = (size_t)0; 
void *rl_variable = NULL;

__nbrune_t	charsetbits = (__nbrune_t)0x00000000;
#if 0
__nbrune_t	charsetmask = (__nbrune_t)0x0000007f;
#endif
__nbrune_t	charsetmask = (__nbrune_t)0xffffffff;

void set_map __P((rune_map *, rune_list *, u_int32_t));
void set_digitmap __P((rune_map *, rune_list *));
void add_map __P((rune_map *, rune_list *, u_int32_t));

int		main __P((int, char *[]));
void		usage __P((void));
int		yyerror __P((const char *s));
void		*xmalloc __P((unsigned int sz));
u_int32_t	*xlalloc __P((unsigned int sz));
u_int32_t	*xrelalloc __P((u_int32_t *old, unsigned int sz));
void		dump_tables __P((void));
int		yyparse __P((void));
extern int	yylex __P((void));

/* mklocaledb.c */
extern void mklocaledb __P((const char *, FILE *, FILE *));

%}

%union	{
    __nbrune_t	rune;
    int		i;
    char	*str;

    rune_list	*list;
}

%token	<rune>	RUNE
%token		LBRK
%token		RBRK
%token		THRU
%token		MAPLOWER
%token		MAPUPPER
%token		DIGITMAP
%token	<i>	LIST
%token	<str>	VARIABLE
%token		CHARSET
%token		ENCODING
%token		INVALID
%token	<str>	STRING

%type	<list>	list
%type	<list>	map


%%

locale	:	/* empty */
	|	table
	    	{ dump_tables(); }
	;

table	:	entry
	|	table entry
	;

entry	:	ENCODING STRING
		{ strncpy(new_locale.frl_encoding, $2, sizeof(new_locale.frl_encoding)); }
	|	VARIABLE
		{ rl_variable_len = strlen($1) + 1;
		  rl_variable = strdup($1);
		  new_locale.frl_variable_len = htonl((u_int32_t)rl_variable_len);
		}
	|	CHARSET RUNE
		{ charsetbits = $2; charsetmask = 0x0000007f; }
	|	CHARSET RUNE RUNE
		{ charsetbits = $2; charsetmask = $3; }
	|	CHARSET STRING
		{ int final = $2[strlen($2) - 1] & 0x7f;
		  charsetbits = final << 24;
		  if ($2[0] == '$') {
			charsetmask = 0x00007f7f;
			if (strchr(",-./", $2[1]))
				charsetbits |= 0x80;
			if (0xd0 <= final && final <= 0xdf)
				charsetmask |= 0x007f0000;
		  } else {
			charsetmask = 0x0000007f;
			if (strchr(",-./", $2[0]))
				charsetbits |= 0x80;
			if (strlen($2) == 2 && $2[0] == '!')
				charsetbits |= ((0x80 | $2[0]) << 16);
		  }

		  /*
		   * special rules
		   */
		  if (charsetbits == ('B' << 24)
		   && charsetmask == 0x0000007f) {
			/*ASCII: 94B*/
			charsetbits = 0;
			charsetmask = 0x0000007f;
		  } else if (charsetbits == (('A' << 24) | 0x80)
		  	  && charsetmask == 0x0000007f) {
		  	/*Latin1: 96A*/
			charsetbits = 0x80;
			charsetmask = 0x0000007f;
		  }
		}
	|	INVALID RUNE
		{ new_locale.frl_invalid_rune = htonl((u_int32_t)$2); }
	|	LIST list
		{ set_map(&types, $2, $1); }
	|	MAPLOWER map
		{ set_map(&maplower, $2, 0); }
	|	MAPUPPER map
		{ set_map(&mapupper, $2, 0); }
	|	DIGITMAP map
		{ set_digitmap(&types, $2); }
	;

list	:	RUNE
		{
		    $$ = (rune_list *)malloc(sizeof(rune_list));
		    $$->min = ($1 & charsetmask) | charsetbits;
		    $$->max = ($1 & charsetmask) | charsetbits;
		    $$->next = 0;
		}
	|	RUNE THRU RUNE
		{
		    $$ = (rune_list *)malloc(sizeof(rune_list));
		    $$->min = ($1 & charsetmask) | charsetbits;
		    $$->max = ($3 & charsetmask) | charsetbits;
		    $$->next = 0;
		}
	|	list RUNE
		{
		    $$ = (rune_list *)malloc(sizeof(rune_list));
		    $$->min = ($2 & charsetmask) | charsetbits;
		    $$->max = ($2 & charsetmask) | charsetbits;
		    $$->next = $1;
		}
	|	list RUNE THRU RUNE
		{
		    $$ = (rune_list *)malloc(sizeof(rune_list));
		    $$->min = ($2 & charsetmask) | charsetbits;
		    $$->max = ($4 & charsetmask) | charsetbits;
		    $$->next = $1;
		}
	;

map	:	LBRK RUNE RUNE RBRK
		{
		    $$ = (rune_list *)malloc(sizeof(rune_list));
		    $$->min = ($2 & charsetmask) | charsetbits;
		    $$->max = ($2 & charsetmask) | charsetbits;
		    $$->map = $3;
		    $$->next = 0;
		}
	|	map LBRK RUNE RUNE RBRK
		{
		    $$ = (rune_list *)malloc(sizeof(rune_list));
		    $$->min = ($3 & charsetmask) | charsetbits;
		    $$->max = ($3 & charsetmask) | charsetbits;
		    $$->map = $4;
		    $$->next = $1;
		}
	|	LBRK RUNE THRU RUNE ':' RUNE RBRK
		{
		    $$ = (rune_list *)malloc(sizeof(rune_list));
		    $$->min = ($2 & charsetmask) | charsetbits;
		    $$->max = ($4 & charsetmask) | charsetbits;
		    $$->map = $6;
		    $$->next = 0;
		}
	|	map LBRK RUNE THRU RUNE ':' RUNE RBRK
		{
		    $$ = (rune_list *)malloc(sizeof(rune_list));
		    $$->min = ($3 & charsetmask) | charsetbits;
		    $$->max = ($5 & charsetmask) | charsetbits;
		    $$->map = $7;
		    $$->next = $1;
		}
	;
%%

int debug = 0;
FILE *ofile;

int
main(ac, av)
	int ac;
	char *av[];
{
    int x;
    const char *locale_type;

    extern char *optarg;
    extern int optind;

    locale_type = NULL;
    while ((x = getopt(ac, av, "do:t:")) != EOF) {
	switch(x) {
	case 'd':
	    debug = 1;
	    break;
	case 'o':
	    locale_file = optarg;
	    if ((ofile = fopen(locale_file, "w")) == 0)
		err(1, "unable to open output file %s", locale_file);
	    break;
        case 't':
	    locale_type = optarg;
            break;
	default:
	    usage();
	}
    }

    switch (ac - optind) {
    case 0:
	break;
    case 1:
	if (freopen(av[optind], "r", stdin) == 0)
	    err(1, "unable to open input file %s", av[optind]);
	break;
    default:
	usage();
    }

    if (ofile == NULL)
	ofile = stdout;
    if (locale_type != NULL && strcasecmp(locale_type, "CTYPE")) {
	mklocaledb(locale_type, stdin, ofile);
	return 0;
    }

    for (x = 0; x < _CTYPE_CACHE_SIZE; ++x) {
	mapupper.map[x] = x;
	maplower.map[x] = x;
    }

    new_locale.frl_invalid_rune = htonl((u_int32_t)_DEFAULT_INVALID_RUNE);
    memcpy(new_locale.frl_magic, _RUNECT10_MAGIC, sizeof(new_locale.frl_magic));

    yyparse();

    return 0;

}

void
usage()
{
    fprintf(stderr,
	"usage: mklocale [-d] [-o output] [-t type] [source]\n");

    exit(1);
}

int
yyerror(s)
	const char *s;
{
    fprintf(stderr, "%s\n", s);

    return 0;
}

void *
xmalloc(sz)
	unsigned int sz;
{
    void *r = malloc(sz);
    if (!r) {
	perror("xmalloc");
	abort();
    }
    return(r);
}

u_int32_t *
xlalloc(sz)
	unsigned int sz;
{
    u_int32_t *r = (u_int32_t *)malloc(sz * sizeof(u_int32_t));
    if (!r) {
	perror("xlalloc");
	abort();
    }
    return(r);
}

u_int32_t *
xrelalloc(old, sz)
	u_int32_t *old;
	unsigned int sz;
{
    u_int32_t *r = (u_int32_t *)realloc((char *)old,
						sz * sizeof(u_int32_t));
    if (!r) {
	perror("xrelalloc");
	abort();
    }
    return(r);
}

void
set_map(map, list, flag)
	rune_map *map;
	rune_list *list;
	u_int32_t flag;
{
    list->map &= charsetmask;
    list->map |= charsetbits;
    while (list) {
	rune_list *nlist = list->next;
	add_map(map, list, flag);
	list = nlist;
    }
}

void
set_digitmap(map, list)
	rune_map *map;
	rune_list *list;
{
    __nbrune_t i;

    while (list) {
	rune_list *nlist = list->next;
	for (i = list->min; i <= list->max; ++i) {
	    if (list->map + (i - list->min)) {
		rune_list *tmp = (rune_list *)xmalloc(sizeof(rune_list));
		tmp->min = i;
		tmp->max = i;
		add_map(map, tmp, list->map + (i - list->min));
	    }
	}
	free(list);
	list = nlist;
    }
}

void
add_map(map, list, flag)
	rune_map *map;
	rune_list *list;
	u_int32_t flag;
{
    __nbrune_t i;
    rune_list *lr = 0;
    rune_list *r;
    __nbrune_t run;

    while (list->min < _CTYPE_CACHE_SIZE && list->min <= list->max) {
	if (flag)
	    map->map[list->min++] |= flag;
	else
	    map->map[list->min++] = list->map++;
    }

    if (list->min > list->max) {
	free(list);
	return;
    }

    run = list->max - list->min + 1;

    if (!(r = map->root) || (list->max < r->min - 1)
			 || (!flag && list->max == r->min - 1)) {
	if (flag) {
	    list->types = xlalloc(run);
	    for (i = 0; i < run; ++i)
		list->types[i] = flag;
	}
	list->next = map->root;
	map->root = list;
	return;
    }

    for (r = map->root; r && r->max + 1 < list->min; r = r->next)
	lr = r;

    if (!r) {
	/*
	 * We are off the end.
	 */
	if (flag) {
	    list->types = xlalloc(run);
	    for (i = 0; i < run; ++i)
		list->types[i] = flag;
	}
	list->next = 0;
	lr->next = list;
	return;
    }

    if (list->max < r->min - 1) {
	/*
	 * We come before this range and we do not intersect it.
	 * We are not before the root node, it was checked before the loop
	 */
	if (flag) {
	    list->types = xlalloc(run);
	    for (i = 0; i < run; ++i)
		list->types[i] = flag;
	}
	list->next = lr->next;
	lr->next = list;
	return;
    }

    /*
     * At this point we have found that we at least intersect with
     * the range pointed to by `r', we might intersect with one or
     * more ranges beyond `r' as well.
     */

    if (!flag && list->map - list->min != r->map - r->min) {
	/*
	 * There are only two cases when we are doing case maps and
	 * our maps needn't have the same offset.  When we are adjoining
	 * but not intersecting.
	 */
	if (list->max + 1 == r->min) {
	    lr->next = list;
	    list->next = r;
	    return;
	}
	if (list->min - 1 == r->max) {
	    list->next = r->next;
	    r->next = list;
	    return;
	}
	fprintf(stderr, "Error: conflicting map entries\n");
	exit(1);
    }

    if (list->min >= r->min && list->max <= r->max) {
	/*
	 * Subset case.
	 */

	if (flag) {
	    for (i = list->min; i <= list->max; ++i)
		r->types[i - r->min] |= flag;
	}
	free(list);
	return;
    }
    if (list->min <= r->min && list->max >= r->max) {
	/*
	 * Superset case.  Make him big enough to hold us.
	 * We might need to merge with the guy after him.
	 */
	if (flag) {
	    list->types = xlalloc(list->max - list->min + 1);

	    for (i = list->min; i <= list->max; ++i)
		list->types[i - list->min] = flag;

	    for (i = r->min; i <= r->max; ++i)
		list->types[i - list->min] |= r->types[i - r->min];

	    free(r->types);
	    r->types = list->types;
	} else {
	    r->map = list->map;
	}
	r->min = list->min;
	r->max = list->max;
	free(list);
    } else if (list->min < r->min) {
	/*
	 * Our tail intersects his head.
	 */
	if (flag) {
	    list->types = xlalloc(r->max - list->min + 1);

	    for (i = r->min; i <= r->max; ++i)
		list->types[i - list->min] = r->types[i - r->min];

	    for (i = list->min; i < r->min; ++i)
		list->types[i - list->min] = flag;

	    for (i = r->min; i <= list->max; ++i)
		list->types[i - list->min] |= flag;

	    free(r->types);
	    r->types = list->types;
	} else {
	    r->map = list->map;
	}
	r->min = list->min;
	free(list);
	return;
    } else {
	/*
	 * Our head intersects his tail.
	 * We might need to merge with the guy after him.
	 */
	if (flag) {
	    r->types = xrelalloc(r->types, list->max - r->min + 1);

	    for (i = list->min; i <= r->max; ++i)
		r->types[i - r->min] |= flag;

	    for (i = r->max+1; i <= list->max; ++i)
		r->types[i - r->min] = flag;
	}
	r->max = list->max;
	free(list);
    }

    /*
     * Okay, check to see if we grew into the next guy(s)
     */
    while ((lr = r->next) && r->max >= lr->min) {
	if (flag) {
	    if (r->max >= lr->max) {
		/*
		 * Good, we consumed all of him.
		 */
		for (i = lr->min; i <= lr->max; ++i)
		    r->types[i - r->min] |= lr->types[i - lr->min];
	    } else {
		/*
		 * "append" him on to the end of us.
		 */
		r->types = xrelalloc(r->types, lr->max - r->min + 1);

		for (i = lr->min; i <= r->max; ++i)
		    r->types[i - r->min] |= lr->types[i - lr->min];

		for (i = r->max+1; i <= lr->max; ++i)
		    r->types[i - r->min] = lr->types[i - lr->min];

		r->max = lr->max;
	    }
	} else {
	    if (lr->max > r->max)
		r->max = lr->max;
	}

	r->next = lr->next;

	if (flag)
	    free(lr->types);
	free(lr);
    }
}

void
dump_tables()
{
    int x, n;
    rune_list *list;
    FILE *fp = ofile;
    u_int32_t nranges;

    /*
     * See if we can compress some of the istype arrays
     */
    for(list = types.root; list; list = list->next) {
	list->map = list->types[0];
	for (x = 1; x < list->max - list->min + 1; ++x) {
	    if (list->types[x] != list->map) {
		list->map = 0;
		break;
	    }
	}
    }

    /*
     * Fill in our tables.  Do this in network order so that
     * diverse machines have a chance of sharing data.
     * (Machines like Crays cannot share with little machines due to
     *  word size.  Sigh.  We tried.)
     */
    for (x = 0; x < _CTYPE_CACHE_SIZE; ++x) {
	new_locale.frl_runetype[x] = htonl(types.map[x]);
	new_locale.frl_maplower[x] = htonl(maplower.map[x]);
	new_locale.frl_mapupper[x] = htonl(mapupper.map[x]);
    }

    /*
     * Count up how many ranges we will need for each of the extents.
     */
    list = types.root;

    nranges = (u_int32_t)0;
    while (list) {
	++nranges;
	list = list->next;
    }
    new_locale.frl_runetype_ext.frr_nranges =
	htonl(nranges);

    list = maplower.root;

    nranges = (u_int32_t)0;
    while (list) {
	++nranges;
	list = list->next;
    }
    new_locale.frl_maplower_ext.frr_nranges =
	htonl(nranges);

    list = mapupper.root;

    nranges = (u_int32_t)0;
    while (list) {
	++nranges;
	list = list->next;
    }
    new_locale.frl_mapupper_ext.frr_nranges =
	htonl(nranges);

    /*
     * Okay, we are now ready to write the new locale file.
     */

    /*
     * PART 1: The _RuneLocale structure
     */
    if (fwrite((char *)&new_locale, sizeof(new_locale), 1, fp) != 1)
	err(1, "writing _FileRuneLocale to %s", locale_file);
    /*
     * PART 2: The runetype_ext structures (not the actual tables)
     */
    for (list = types.root, n = 0; list != NULL; list = list->next, n++) {
	_FileRuneEntry re;

	memset(&re, 0, sizeof(re));
	re.fre_min = htonl(list->min);
	re.fre_max = htonl(list->max);
	re.fre_map = htonl(list->map);

	if (fwrite((char *)&re, sizeof(re), 1, fp) != 1)
	    err(1, "writing runetype_ext #%d to %s", n, locale_file);
    }
    /*
     * PART 3: The maplower_ext structures
     */
    for (list = maplower.root, n = 0; list != NULL; list = list->next, n++) {
	_FileRuneEntry re;

	memset(&re, 0, sizeof(re));
	re.fre_min = htonl(list->min);
	re.fre_max = htonl(list->max);
	re.fre_map = htonl(list->map);

	if (fwrite((char *)&re, sizeof(re), 1, fp) != 1)
	    err(1, "writing maplower_ext #%d to %s", n, locale_file);
    }
    /*
     * PART 4: The mapupper_ext structures
     */
    for (list = mapupper.root, n = 0; list != NULL; list = list->next, n++) {
	_FileRuneEntry re;

	memset(&re, 0, sizeof(re));
	re.fre_min = htonl(list->min);
	re.fre_max = htonl(list->max);
	re.fre_map = htonl(list->map);

	if (fwrite((char *)&re, sizeof(re), 1, fp) != 1)
	    err(1, "writing mapupper_ext #%d to %s", n, locale_file);
    }
    /*
     * PART 5: The runetype_ext tables
     */
    for (list = types.root, n = 0; list != NULL; list = list->next, n++) {
	for (x = 0; x < list->max - list->min + 1; ++x)
	    list->types[x] = htonl(list->types[x]);

	if (!list->map) {
	    if (fwrite((char *)list->types,
		       (list->max - list->min + 1) * sizeof(u_int32_t),
		       1, fp) != 1)
		err(1, "writing runetype_ext table #%d to %s", n, locale_file);
	}
    }
    /*
     * PART 5: And finally the variable data
     */
    if (rl_variable_len != 0 &&
	fwrite((char *)rl_variable, rl_variable_len, 1, fp) != 1)
	err(1, "writing variable data to %s", locale_file);
    fclose(fp);

    if (!debug)
	return;

    if (new_locale.frl_encoding[0])
	fprintf(stderr, "ENCODING	%.*s\n",
	    (int)sizeof(new_locale.frl_encoding), new_locale.frl_encoding);
    if (rl_variable)
	fprintf(stderr, "VARIABLE	%.*s\n",
	    (int)rl_variable_len, (char *)rl_variable);

    fprintf(stderr, "\nMAPLOWER:\n\n");

    for (x = 0; x < _CTYPE_CACHE_SIZE; ++x) {
	if (isprint(maplower.map[x]))
	    fprintf(stderr, " '%c'", (int)maplower.map[x]);
	else if (maplower.map[x])
	    fprintf(stderr, "%04x", maplower.map[x]);
	else
	    fprintf(stderr, "%4x", 0);
	if ((x & 0xf) == 0xf)
	    fprintf(stderr, "\n");
	else
	    fprintf(stderr, " ");
    }
    fprintf(stderr, "\n");

    for (list = maplower.root; list; list = list->next)
	fprintf(stderr, "\t%04x - %04x : %04x\n", list->min, list->max, list->map);

    fprintf(stderr, "\nMAPUPPER:\n\n");

    for (x = 0; x < _CTYPE_CACHE_SIZE; ++x) {
	if (isprint(mapupper.map[x]))
	    fprintf(stderr, " '%c'", (int)mapupper.map[x]);
	else if (mapupper.map[x])
	    fprintf(stderr, "%04x", mapupper.map[x]);
	else
	    fprintf(stderr, "%4x", 0);
	if ((x & 0xf) == 0xf)
	    fprintf(stderr, "\n");
	else
	    fprintf(stderr, " ");
    }
    fprintf(stderr, "\n");

    for (list = mapupper.root; list; list = list->next)
	fprintf(stderr, "\t%04x - %04x : %04x\n", list->min, list->max, list->map);


    fprintf(stderr, "\nTYPES:\n\n");

    for (x = 0; x < _CTYPE_CACHE_SIZE; ++x) {
	u_int32_t r = types.map[x];

	if (r) {
	    if (isprint(x))
		fprintf(stderr, " '%c':%2d", x, (int)(r & 0xff));
	    else
		fprintf(stderr, "%04x:%2d", x, (int)(r & 0xff));

	    fprintf(stderr, " %4s", (r & _RUNETYPE_A) ? "alph" : "");
	    fprintf(stderr, " %4s", (r & _RUNETYPE_C) ? "ctrl" : "");
	    fprintf(stderr, " %4s", (r & _RUNETYPE_D) ? "dig" : "");
	    fprintf(stderr, " %4s", (r & _RUNETYPE_G) ? "graf" : "");
	    fprintf(stderr, " %4s", (r & _RUNETYPE_L) ? "low" : "");
	    fprintf(stderr, " %4s", (r & _RUNETYPE_P) ? "punc" : "");
	    fprintf(stderr, " %4s", (r & _RUNETYPE_S) ? "spac" : "");
	    fprintf(stderr, " %4s", (r & _RUNETYPE_U) ? "upp" : "");
	    fprintf(stderr, " %4s", (r & _RUNETYPE_X) ? "xdig" : "");
	    fprintf(stderr, " %4s", (r & _RUNETYPE_B) ? "blnk" : "");
	    fprintf(stderr, " %4s", (r & _RUNETYPE_R) ? "prnt" : "");
	    fprintf(stderr, " %4s", (r & _RUNETYPE_I) ? "ideo" : "");
	    fprintf(stderr, " %4s", (r & _RUNETYPE_T) ? "spec" : "");
	    fprintf(stderr, " %4s", (r & _RUNETYPE_Q) ? "phon" : "");
	    fprintf(stderr, "\n");
	}
    }

    for (list = types.root; list; list = list->next) {
	if (list->map && list->min + 3 < list->max) {
	    u_int32_t r = list->map;

	    fprintf(stderr, "%04x:%2d", list->min, r & 0xff);

	    fprintf(stderr, " %4s", (r & _RUNETYPE_A) ? "alph" : "");
	    fprintf(stderr, " %4s", (r & _RUNETYPE_C) ? "ctrl" : "");
	    fprintf(stderr, " %4s", (r & _RUNETYPE_D) ? "dig" : "");
	    fprintf(stderr, " %4s", (r & _RUNETYPE_G) ? "graf" : "");
	    fprintf(stderr, " %4s", (r & _RUNETYPE_L) ? "low" : "");
	    fprintf(stderr, " %4s", (r & _RUNETYPE_P) ? "punc" : "");
	    fprintf(stderr, " %4s", (r & _RUNETYPE_S) ? "spac" : "");
	    fprintf(stderr, " %4s", (r & _RUNETYPE_U) ? "upp" : "");
	    fprintf(stderr, " %4s", (r & _RUNETYPE_X) ? "xdig" : "");
	    fprintf(stderr, " %4s", (r & _RUNETYPE_B) ? "blnk" : "");
	    fprintf(stderr, " %4s", (r & _RUNETYPE_R) ? "prnt" : "");
	    fprintf(stderr, " %4s", (r & _RUNETYPE_I) ? "ideo" : "");
	    fprintf(stderr, " %4s", (r & _RUNETYPE_T) ? "spec" : "");
	    fprintf(stderr, " %4s", (r & _RUNETYPE_Q) ? "phon" : "");
	    fprintf(stderr, "\n...\n");

	    fprintf(stderr, "%04x:%2d", list->max, r & 0xff);

	    fprintf(stderr, " %4s", (r & _RUNETYPE_A) ? "alph" : "");
	    fprintf(stderr, " %4s", (r & _RUNETYPE_C) ? "ctrl" : "");
	    fprintf(stderr, " %4s", (r & _RUNETYPE_D) ? "dig" : "");
	    fprintf(stderr, " %4s", (r & _RUNETYPE_G) ? "graf" : "");
	    fprintf(stderr, " %4s", (r & _RUNETYPE_L) ? "low" : "");
	    fprintf(stderr, " %4s", (r & _RUNETYPE_P) ? "punc" : "");
	    fprintf(stderr, " %4s", (r & _RUNETYPE_S) ? "spac" : "");
	    fprintf(stderr, " %4s", (r & _RUNETYPE_U) ? "upp" : "");
	    fprintf(stderr, " %4s", (r & _RUNETYPE_X) ? "xdig" : "");
	    fprintf(stderr, " %4s", (r & _RUNETYPE_B) ? "blnk" : "");
	    fprintf(stderr, " %4s", (r & _RUNETYPE_R) ? "prnt" : "");
	    fprintf(stderr, " %4s", (r & _RUNETYPE_I) ? "ideo" : "");
	    fprintf(stderr, " %4s", (r & _RUNETYPE_T) ? "spec" : "");
	    fprintf(stderr, " %4s", (r & _RUNETYPE_Q) ? "phon" : "");
            fprintf(stderr, " %1u", (unsigned)((r & _RUNETYPE_SWM)>>_RUNETYPE_SWS));
	    fprintf(stderr, "\n");
	} else 
	for (x = list->min; x <= list->max; ++x) {
	    u_int32_t r = ntohl(list->types[x - list->min]);

	    if (r) {
		fprintf(stderr, "%04x:%2d", x, (int)(r & 0xff));

		fprintf(stderr, " %4s", (r & _RUNETYPE_A) ? "alph" : "");
		fprintf(stderr, " %4s", (r & _RUNETYPE_C) ? "ctrl" : "");
		fprintf(stderr, " %4s", (r & _RUNETYPE_D) ? "dig" : "");
		fprintf(stderr, " %4s", (r & _RUNETYPE_G) ? "graf" : "");
		fprintf(stderr, " %4s", (r & _RUNETYPE_L) ? "low" : "");
		fprintf(stderr, " %4s", (r & _RUNETYPE_P) ? "punc" : "");
		fprintf(stderr, " %4s", (r & _RUNETYPE_S) ? "spac" : "");
		fprintf(stderr, " %4s", (r & _RUNETYPE_U) ? "upp" : "");
		fprintf(stderr, " %4s", (r & _RUNETYPE_X) ? "xdig" : "");
		fprintf(stderr, " %4s", (r & _RUNETYPE_B) ? "blnk" : "");
		fprintf(stderr, " %4s", (r & _RUNETYPE_R) ? "prnt" : "");
		fprintf(stderr, " %4s", (r & _RUNETYPE_I) ? "ideo" : "");
		fprintf(stderr, " %4s", (r & _RUNETYPE_T) ? "spec" : "");
		fprintf(stderr, " %4s", (r & _RUNETYPE_Q) ? "phon" : "");
                fprintf(stderr, " %1u", (unsigned)((r & _RUNETYPE_SWM)>>_RUNETYPE_SWS));
		fprintf(stderr, "\n");
	    }
	}
    }
}
