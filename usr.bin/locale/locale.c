/*	$NetBSD: locale.c,v 1.8 2012/01/20 16:31:30 joerg Exp $	*/

/*-
 * Copyright (c) 2002, 2003 Alexey Zelkin <phantom@FreeBSD.org>
 * All rights reserved.
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
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE AUTHOR OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 *
 * FreeBSD: src/usr.bin/locale/locale.c,v 1.10 2003/06/26 11:05:56 phantom Exp
 */

#include <sys/cdefs.h>
#if defined(LIBC_SCCS) && !defined(lint)
__RCSID("$NetBSD: locale.c,v 1.8 2012/01/20 16:31:30 joerg Exp $");
#endif /* LIBC_SCCS and not lint */

/*
 * XXX: implement missing era_* (LC_TIME) keywords (require libc &
 *	nl_langinfo(3) extensions)
 *
 * XXX: correctly handle reserved 'charmap' keyword and '-m' option (require
 *      localedef(1) implementation).  Currently it's handled via
 *	nl_langinfo(CODESET).
 */

#include <sys/types.h>
#include <assert.h>
#include <dirent.h>
#include <err.h>
#include <locale.h>
#include <langinfo.h>
#include <limits.h>
#include <paths.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stringlist.h>
#include <unistd.h>

#include "citrus_namespace.h"
#include "citrus_region.h"
#include "citrus_lookup.h"
#include "setlocale_local.h"

/* Local prototypes */
void	init_locales_list(void);
void	init_locales_list_alias(void);
void	list_charmaps(void);
void	list_locales(void);
const char *lookup_localecat(int);
char	*kwval_lconv(int);
int	kwval_lookup(char *, char **, int *, int *);
void	showdetails(char *);
void	showkeywordslist(void);
void	showlocale(void);
void	usage(void);

/* Global variables */
static StringList *locales = NULL;

int	all_locales = 0;
int	all_charmaps = 0;
int	prt_categories = 0;
int	prt_keywords = 0;
int	more_params = 0;

struct _lcinfo {
	const char	*name;
	int		id;
} lcinfo [] = {
	{ "LC_CTYPE",		LC_CTYPE },
	{ "LC_COLLATE",		LC_COLLATE },
	{ "LC_TIME",		LC_TIME },
	{ "LC_NUMERIC",		LC_NUMERIC },
	{ "LC_MONETARY",	LC_MONETARY },
	{ "LC_MESSAGES",	LC_MESSAGES }
};
#define NLCINFO (sizeof(lcinfo)/sizeof(lcinfo[0]))

/* ids for values not referenced by nl_langinfo() */
#define	KW_ZERO			10000
#define	KW_GROUPING		(KW_ZERO+1)
#define KW_INT_CURR_SYMBOL 	(KW_ZERO+2)
#define KW_CURRENCY_SYMBOL 	(KW_ZERO+3)
#define KW_MON_DECIMAL_POINT 	(KW_ZERO+4)
#define KW_MON_THOUSANDS_SEP 	(KW_ZERO+5)
#define KW_MON_GROUPING 	(KW_ZERO+6)
#define KW_POSITIVE_SIGN 	(KW_ZERO+7)
#define KW_NEGATIVE_SIGN 	(KW_ZERO+8)
#define KW_INT_FRAC_DIGITS 	(KW_ZERO+9)
#define KW_FRAC_DIGITS 		(KW_ZERO+10)
#define KW_P_CS_PRECEDES 	(KW_ZERO+11)
#define KW_P_SEP_BY_SPACE 	(KW_ZERO+12)
#define KW_N_CS_PRECEDES 	(KW_ZERO+13)
#define KW_N_SEP_BY_SPACE 	(KW_ZERO+14)
#define KW_P_SIGN_POSN 		(KW_ZERO+15)
#define KW_N_SIGN_POSN 		(KW_ZERO+16)
#define KW_INT_P_CS_PRECEDES 	(KW_ZERO+17)
#define KW_INT_P_SEP_BY_SPACE 	(KW_ZERO+18)
#define KW_INT_N_CS_PRECEDES 	(KW_ZERO+19)
#define KW_INT_N_SEP_BY_SPACE 	(KW_ZERO+20)
#define KW_INT_P_SIGN_POSN 	(KW_ZERO+21)
#define KW_INT_N_SIGN_POSN 	(KW_ZERO+22)

struct _kwinfo {
	const char	*name;
	int		isstr;		/* true - string, false - number */
	int		catid;		/* LC_* */
	int		value_ref;
	const char	*comment;
} kwinfo [] = {
	{ "charmap",		1, LC_CTYPE,	CODESET, "" },	/* hack */

	{ "decimal_point",	1, LC_NUMERIC,	RADIXCHAR, "" },
	{ "thousands_sep",	1, LC_NUMERIC,	THOUSEP, "" },
	{ "grouping",		1, LC_NUMERIC,	KW_GROUPING, "" },
	{ "radixchar",		1, LC_NUMERIC,	RADIXCHAR,
	  "Same as decimal_point (BSD only)" },			/* compat */
	{ "thousep",		1, LC_NUMERIC,	THOUSEP,
	  "Same as thousands_sep (BSD only)" },			/* compat */

	{ "int_curr_symbol",	1, LC_MONETARY,	KW_INT_CURR_SYMBOL, "" },
	{ "currency_symbol",	1, LC_MONETARY,	KW_CURRENCY_SYMBOL, "" },
	{ "mon_decimal_point",	1, LC_MONETARY,	KW_MON_DECIMAL_POINT, "" },
	{ "mon_thousands_sep",	1, LC_MONETARY,	KW_MON_THOUSANDS_SEP, "" },
	{ "mon_grouping",	1, LC_MONETARY,	KW_MON_GROUPING, "" },
	{ "positive_sign",	1, LC_MONETARY,	KW_POSITIVE_SIGN, "" },
	{ "negative_sign",	1, LC_MONETARY,	KW_NEGATIVE_SIGN, "" },

	{ "int_frac_digits",	0, LC_MONETARY,	KW_INT_FRAC_DIGITS, "" },
	{ "frac_digits",	0, LC_MONETARY,	KW_FRAC_DIGITS, "" },
	{ "p_cs_precedes",	0, LC_MONETARY,	KW_P_CS_PRECEDES, "" },
	{ "p_sep_by_space",	0, LC_MONETARY,	KW_P_SEP_BY_SPACE, "" },
	{ "n_cs_precedes",	0, LC_MONETARY,	KW_N_CS_PRECEDES, "" },
	{ "n_sep_by_space",	0, LC_MONETARY,	KW_N_SEP_BY_SPACE, "" },
	{ "p_sign_posn",	0, LC_MONETARY,	KW_P_SIGN_POSN, "" },
	{ "n_sign_posn",	0, LC_MONETARY,	KW_N_SIGN_POSN, "" },
	{ "int_p_cs_precedes",	0, LC_MONETARY,	KW_INT_P_CS_PRECEDES, "" },
	{ "int_p_sep_by_space",	0, LC_MONETARY,	KW_INT_P_SEP_BY_SPACE, "" },
	{ "int_n_cs_precedes",	0, LC_MONETARY,	KW_INT_N_CS_PRECEDES, "" },
	{ "int_n_sep_by_space",	0, LC_MONETARY,	KW_INT_N_SEP_BY_SPACE, "" },
	{ "int_p_sign_posn",	0, LC_MONETARY,	KW_INT_P_SIGN_POSN, "" },
	{ "int_n_sign_posn",	0, LC_MONETARY,	KW_INT_N_SIGN_POSN, "" },

	{ "d_t_fmt",		1, LC_TIME,	D_T_FMT, "" },
	{ "d_fmt",		1, LC_TIME,	D_FMT, "" },
	{ "t_fmt",		1, LC_TIME,	T_FMT, "" },
	{ "am_str",		1, LC_TIME,	AM_STR, "" },
	{ "pm_str",		1, LC_TIME,	PM_STR, "" },
	{ "t_fmt_ampm",		1, LC_TIME,	T_FMT_AMPM, "" },
	{ "day_1",		1, LC_TIME,	DAY_1, "" },
	{ "day_2",		1, LC_TIME,	DAY_2, "" },
	{ "day_3",		1, LC_TIME,	DAY_3, "" },
	{ "day_4",		1, LC_TIME,	DAY_4, "" },
	{ "day_5",		1, LC_TIME,	DAY_5, "" },
	{ "day_6",		1, LC_TIME,	DAY_6, "" },
	{ "day_7",		1, LC_TIME,	DAY_7, "" },
	{ "abday_1",		1, LC_TIME,	ABDAY_1, "" },
	{ "abday_2",		1, LC_TIME,	ABDAY_2, "" },
	{ "abday_3",		1, LC_TIME,	ABDAY_3, "" },
	{ "abday_4",		1, LC_TIME,	ABDAY_4, "" },
	{ "abday_5",		1, LC_TIME,	ABDAY_5, "" },
	{ "abday_6",		1, LC_TIME,	ABDAY_6, "" },
	{ "abday_7",		1, LC_TIME,	ABDAY_7, "" },
	{ "mon_1",		1, LC_TIME,	MON_1, "" },
	{ "mon_2",		1, LC_TIME,	MON_2, "" },
	{ "mon_3",		1, LC_TIME,	MON_3, "" },
	{ "mon_4",		1, LC_TIME,	MON_4, "" },
	{ "mon_5",		1, LC_TIME,	MON_5, "" },
	{ "mon_6",		1, LC_TIME,	MON_6, "" },
	{ "mon_7",		1, LC_TIME,	MON_7, "" },
	{ "mon_8",		1, LC_TIME,	MON_8, "" },
	{ "mon_9",		1, LC_TIME,	MON_9, "" },
	{ "mon_10",		1, LC_TIME,	MON_10, "" },
	{ "mon_11",		1, LC_TIME,	MON_11, "" },
	{ "mon_12",		1, LC_TIME,	MON_12, "" },
	{ "abmon_1",		1, LC_TIME,	ABMON_1, "" },
	{ "abmon_2",		1, LC_TIME,	ABMON_2, "" },
	{ "abmon_3",		1, LC_TIME,	ABMON_3, "" },
	{ "abmon_4",		1, LC_TIME,	ABMON_4, "" },
	{ "abmon_5",		1, LC_TIME,	ABMON_5, "" },
	{ "abmon_6",		1, LC_TIME,	ABMON_6, "" },
	{ "abmon_7",		1, LC_TIME,	ABMON_7, "" },
	{ "abmon_8",		1, LC_TIME,	ABMON_8, "" },
	{ "abmon_9",		1, LC_TIME,	ABMON_9, "" },
	{ "abmon_10",		1, LC_TIME,	ABMON_10, "" },
	{ "abmon_11",		1, LC_TIME,	ABMON_11, "" },
	{ "abmon_12",		1, LC_TIME,	ABMON_12, "" },
	{ "era",		1, LC_TIME,	ERA, "(unavailable)" },
	{ "era_d_fmt",		1, LC_TIME,	ERA_D_FMT, "(unavailable)" },
	{ "era_d_t_fmt",	1, LC_TIME,	ERA_D_T_FMT, "(unavailable)" },
	{ "era_t_fmt",		1, LC_TIME,	ERA_T_FMT, "(unavailable)" },
	{ "alt_digits",		1, LC_TIME,	ALT_DIGITS, "" },

	{ "yesexpr",		1, LC_MESSAGES, YESEXPR, "" },
	{ "noexpr",		1, LC_MESSAGES, NOEXPR, "" },
	{ "yesstr",		1, LC_MESSAGES, YESSTR,
	  "(POSIX legacy)" },					/* compat */
	{ "nostr",		1, LC_MESSAGES, NOSTR,
	  "(POSIX legacy)" }					/* compat */

};
#define NKWINFO (sizeof(kwinfo)/sizeof(kwinfo[0]))

int
main(int argc, char *argv[])
{
	int	ch;
	int	tmp;

	while ((ch = getopt(argc, argv, "ackm")) != -1) {
		switch (ch) {
		case 'a':
			all_locales = 1;
			break;
		case 'c':
			prt_categories = 1;
			break;
		case 'k':
			prt_keywords = 1;
			break;
		case 'm':
			all_charmaps = 1;
			break;
		default:
			usage();
		}
	}
	argc -= optind;
	argv += optind;

	/* validate arguments */
	if (all_locales && all_charmaps)
		usage();
	if ((all_locales || all_charmaps) && argc > 0) 
		usage();
	if ((all_locales || all_charmaps) && (prt_categories || prt_keywords))
		usage();
	if ((prt_categories || prt_keywords) && argc <= 0)
		usage();

	/* process '-a' */
	if (all_locales) {
		list_locales();
		exit(0);
	}

	/* process '-m' */
	if (all_charmaps) {
		list_charmaps();
		exit(0);
	}

	/* check for special case '-k list' */
	tmp = 0;
	if (prt_keywords && argc > 0)
		while (tmp < argc)
			if (strcasecmp(argv[tmp++], "list") == 0) {
				showkeywordslist();
				exit(0);
			}

	/* process '-c' and/or '-k' */
	if (prt_categories || prt_keywords || argc > 0) {
		setlocale(LC_ALL, "");
		while (argc > 0) {
			showdetails(*argv);
			argv++;
			argc--;
		}
		exit(0);
	}

	/* no arguments, show current locale state */
	showlocale();

	return (0);
}

void
usage(void)
{
	printf("usage: locale [ -a | -m ]\n"
               "       locale [ -ck ] name ...\n");
	exit(1);
}

/*
 * Output information about all available locales
 *
 * XXX actually output of this function does not guarantee that locale
 *     is really available to application, since it can be broken or
 *     inconsistent thus setlocale() will fail.  Maybe add '-V' function to
 *     also validate these locales?
 */
void
list_locales(void)
{
	size_t i;

	init_locales_list();
	for (i = 0; i < locales->sl_cur; i++) {
		printf("%s\n", locales->sl_str[i]);
	}
}

/*
 * qsort() helper function
 */
static int
scmp(const void *s1, const void *s2)
{
	return strcmp(*(const char **)s1, *(const char **)s2);
}

/*
 * Output information about all available charmaps
 *
 * XXX this function is doing a task in hackish way, i.e. by scaning
 *     list of locales, spliting their codeset part and building list of
 *     them.
 */
void
list_charmaps(void)
{
	size_t i;
	char *s, *cs;
	StringList *charmaps;

	/* initialize StringList */
	charmaps = sl_init();
	if (charmaps == NULL)
		err(1, "could not allocate memory");

	/* fetch locales list */
	init_locales_list();

	/* split codesets and build their list */
	for (i = 0; i < locales->sl_cur; i++) {
		s = locales->sl_str[i];
		if ((cs = strchr(s, '.')) != NULL) {
			cs++;
			if (sl_find(charmaps, cs) == NULL)
				sl_add(charmaps, cs);
		}
	}

	/* add US-ASCII, if not yet added */
	if (sl_find(charmaps, "US-ASCII") == NULL)
		sl_add(charmaps, "US-ASCII");

	/* sort the list */
	qsort(charmaps->sl_str, charmaps->sl_cur, sizeof(char *), scmp);

	/* print results */
	for (i = 0; i < charmaps->sl_cur; i++) {
		printf("%s\n", charmaps->sl_str[i]);
	}
}

/*
 * Retrieve sorted list of system locales (or user locales, if PATH_LOCALE
 * environment variable is set)
 */
void
init_locales_list(void)
{
	DIR *dirp;
	struct dirent *dp;
	char *s;

	/* why call this function twice ? */
	if (locales != NULL)
		return;

	/* initialize StringList */
	locales = sl_init();
	if (locales == NULL)
		err(1, "could not allocate memory");

	/* get actual locales directory name */
	setlocale(LC_CTYPE, "C");
	if (_PathLocale == NULL)
		errx(1, "unable to find locales storage");

	/* open locales directory */
	dirp = opendir(_PathLocale);
	if (dirp == NULL)
		err(1, "could not open directory '%s'", _PathLocale);

	/* scan directory and store its contents except "." and ".." */
	while ((dp = readdir(dirp)) != NULL) {
		/* exclude "." and "..", _LOCALE_ALIAS_NAME */
		if ((dp->d_name[0] != '.' || (dp->d_name[1] != '\0' &&
		    (dp->d_name[1] != '.' ||  dp->d_name[2] != '\0'))) &&
		    strcmp(_LOCALE_ALIAS_NAME, dp->d_name) != 0) {
			s = strdup(dp->d_name);
			if (s == NULL)
				err(1, "could not allocate memory");
			sl_add(locales, s);
		}
	}
	closedir(dirp);

        /* make sure that 'POSIX' and 'C' locales are present in the list.
	 * POSIX 1003.1-2001 requires presence of 'POSIX' name only here, but
         * we also list 'C' for constistency
         */
	if (sl_find(locales, "POSIX") == NULL)
		sl_add(locales, "POSIX");

	if (sl_find(locales, "C") == NULL)
		sl_add(locales, "C");

	init_locales_list_alias();

	/* make output nicer, sort the list */
	qsort(locales->sl_str, locales->sl_cur, sizeof(char *), scmp);
}

void
init_locales_list_alias(void)
{
	char aliaspath[PATH_MAX];
	struct _lookup *hlookup;
	struct _region key, dat;
	size_t n;
	char *s, *t;

	_DIAGASSERT(locales != NULL);
	_DIAGASSERT(_PathLocale != NULL);

	(void)snprintf(aliaspath, sizeof(aliaspath),
		"%s/" _LOCALE_ALIAS_NAME, _PathLocale);

	if (_lookup_seq_open(&hlookup, aliaspath,
	    _LOOKUP_CASE_SENSITIVE) == 0) {
		while (_lookup_seq_next(hlookup, &key, &dat) == 0) {
			n = _region_size((const struct _region *)&key);
			s = _region_head((const struct _region *)&key);
			for (t = s; n > 0 && *s!= '/'; --n, ++s);
			n = (size_t)(s - t);
			s = malloc(n + 1);
			if (s == NULL)
				err(1, "could not allocate memory");
			memcpy(s, t, n);
			s[n] = '\0';
			if (sl_find(locales, s) == NULL)
				sl_add(locales, s);
			else
				free(s);
		}
		_lookup_seq_close(hlookup);
	}
}

/*
 * Show current locale status, depending on environment variables
 */
void
showlocale(void)
{
	size_t	i;
	const char *lang, *vval, *eval;

	setlocale(LC_ALL, "");

	lang = getenv("LANG");
	if (lang == NULL) {
		lang = "";
	}
	printf("LANG=\"%s\"\n", lang);
	/* XXX: if LANG is null, then set it to "C" to get implied values? */

	for (i = 0; i < NLCINFO; i++) {
		vval = setlocale(lcinfo[i].id, NULL);
		eval = getenv(lcinfo[i].name);
		if (eval != NULL && !strcmp(eval, vval)
				&& strcmp(lang, vval)) {
			/*
			 * Appropriate environment variable set, its value
			 * is valid and not overriden by LC_ALL
			 *
			 * XXX: possible side effect: if both LANG and
			 * overriden environment variable are set into same
			 * value, then it'll be assumed as 'implied'
			 */
			printf("%s=\"%s\"\n", lcinfo[i].name, vval);
		} else {
			printf("%s=\"%s\"\n", lcinfo[i].name, vval);
		}
	}

	vval = getenv("LC_ALL");
	if (vval == NULL) {
		vval = "";
	}
	printf("LC_ALL=\"%s\"\n", vval);
}

/*
 * keyword value lookup helper (via localeconv())
 */
char *
kwval_lconv(int id)
{
	struct lconv *lc;
	char *rval;

	rval = NULL;
	lc = localeconv();
	switch (id) {
		case KW_GROUPING:
			rval = lc->grouping;
			break;
		case KW_INT_CURR_SYMBOL:
			rval = lc->int_curr_symbol;
			break;
		case KW_CURRENCY_SYMBOL:
			rval = lc->currency_symbol;
			break;
		case KW_MON_DECIMAL_POINT:
			rval = lc->mon_decimal_point;
			break;
		case KW_MON_THOUSANDS_SEP:
			rval = lc->mon_thousands_sep;
			break;
		case KW_MON_GROUPING:
			rval = lc->mon_grouping;
			break;
		case KW_POSITIVE_SIGN:
			rval = lc->positive_sign;
			break;
		case KW_NEGATIVE_SIGN:
			rval = lc->negative_sign;
			break;
		case KW_INT_FRAC_DIGITS:
			rval = &(lc->int_frac_digits);
			break;
		case KW_FRAC_DIGITS:
			rval = &(lc->frac_digits);
			break;
		case KW_P_CS_PRECEDES:
			rval = &(lc->p_cs_precedes);
			break;
		case KW_P_SEP_BY_SPACE:
			rval = &(lc->p_sep_by_space);
			break;
		case KW_N_CS_PRECEDES:
			rval = &(lc->n_cs_precedes);
			break;
		case KW_N_SEP_BY_SPACE:
			rval = &(lc->n_sep_by_space);
			break;
		case KW_P_SIGN_POSN:
			rval = &(lc->p_sign_posn);
			break;
		case KW_N_SIGN_POSN:
			rval = &(lc->n_sign_posn);
			break;
		case KW_INT_P_CS_PRECEDES:
			rval = &(lc->int_p_cs_precedes);
			break;
		case KW_INT_P_SEP_BY_SPACE:
			rval = &(lc->int_p_sep_by_space);
			break;
		case KW_INT_N_CS_PRECEDES:
			rval = &(lc->int_n_cs_precedes);
			break;
		case KW_INT_N_SEP_BY_SPACE:
			rval = &(lc->int_n_sep_by_space);
			break;
		case KW_INT_P_SIGN_POSN:
			rval = &(lc->int_p_sign_posn);
			break;
		case KW_INT_N_SIGN_POSN:
			rval = &(lc->int_n_sign_posn);
			break;
		default:
			break;
	}
	return (rval);
}

/*
 * keyword value and properties lookup
 */
int
kwval_lookup(char *kwname, char **kwval, int *cat, int *isstr)
{
	int	rval;
	size_t	i;

	rval = 0;
	for (i = 0; i < NKWINFO; i++) {
		if (strcasecmp(kwname, kwinfo[i].name) == 0) {
			rval = 1;
			*cat = kwinfo[i].catid;
			*isstr = kwinfo[i].isstr;
			if (kwinfo[i].value_ref < KW_ZERO) {
				*kwval = nl_langinfo(kwinfo[i].value_ref);
			} else {
				*kwval = kwval_lconv(kwinfo[i].value_ref);
			}
			break;
		}
	}

	return (rval);
}

/*
 * Show details about requested keyword according to '-k' and/or '-c'
 * command line options specified.
 */
void
showdetails(char *kw)
{
	int	isstr, cat, tmpval;
	char	*kwval;

	if (kwval_lookup(kw, &kwval, &cat, &isstr) == 0) {
		/*
		 * invalid keyword specified.
		 * XXX: any actions?
		 */
		return;
	}

	if (prt_categories) {
		printf("%s\n", lookup_localecat(cat));
	}

	if (prt_keywords) {
		if (isstr) {
			printf("%s=\"%s\"\n", kw, kwval);
		} else {
			tmpval = (char) *kwval;
			printf("%s=%d\n", kw, tmpval);
		}
	}

	if (!prt_categories && !prt_keywords) {
		if (isstr) {
			printf("%s\n", kwval);
		} else {
			tmpval = (char) *kwval;
			printf("%d\n", tmpval);
		}
	}
}

/*
 * Convert locale category id into string
 */
const char *
lookup_localecat(int cat)
{
	size_t	i;

	for (i = 0; i < NLCINFO; i++)
		if (lcinfo[i].id == cat) {
			return (lcinfo[i].name);
		}
	return ("UNKNOWN");
}

/*
 * Show list of keywords
 */
void
showkeywordslist(void)
{
	size_t	i;

#define FMT "%-20s %-12s %-7s %-20s\n"

	printf("List of available keywords\n\n");
	printf(FMT, "Keyword", "Category", "Type", "Comment");
	printf("-------------------- ------------ ------- --------------------\n");
	for (i = 0; i < NKWINFO; i++) {
		printf(FMT,
			kwinfo[i].name,
			lookup_localecat(kwinfo[i].catid),
			(kwinfo[i].isstr == 0) ? "number" : "string",
			kwinfo[i].comment);
	}
}
