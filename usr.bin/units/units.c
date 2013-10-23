/*	$NetBSD: units.c,v 1.24 2013/01/06 00:19:13 wiz Exp $	*/

/*
 * units.c   Copyright (c) 1993 by Adrian Mariano (adrian@cam.cornell.edu)
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. The name of the author may not be used to endorse or promote products
 *    derived from this software without specific prior written permission.
 * Disclaimer:  This software is provided by the author "as is".  The author
 * shall not be liable for any damages caused in any way by this software.
 *
 * I would appreciate (though I do not require) receiving a copy of any
 * improvements you might make to this program.
 */

#include <ctype.h>
#include <err.h>
#include <float.h>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <unistd.h>

#include "pathnames.h"

#define VERSION "1.0"

#ifndef UNITSFILE
#define UNITSFILE _PATH_UNITSLIB
#endif

#define MAXUNITS 1000
#define MAXPREFIXES 50

#define MAXSUBUNITS 500

#define PRIMITIVECHAR '!'

static int precision = 8;		/* for printf with "%.*g" format */

static const char *errprefix = NULL;	/* if not NULL, then prepend this
					 * to error messages and send them to
					 * stdout instead of stderr.
					 */

static const char *powerstring = "^";

static struct {
	const char *uname;
	const char *uval;
}      unittable[MAXUNITS];

struct unittype {
	const char *numerator[MAXSUBUNITS];
	const char *denominator[MAXSUBUNITS];
	double factor;
};

struct {
	const char *prefixname;
	const char *prefixval;
}      prefixtable[MAXPREFIXES];


static const char *NULLUNIT = "";

static int unitcount;
static int prefixcount;


static int	addsubunit(const char *[], const char *);
static int	addunit(struct unittype *, const char *, int);
static void	cancelunit(struct unittype *);
static int	compare(const void *, const void *);
static int	compareproducts(const char **, const char **);
static int	compareunits(struct unittype *, struct unittype *);
static int	compareunitsreciprocal(struct unittype *, struct unittype *);
static int	completereduce(struct unittype *);
static void	initializeunit(struct unittype *);
static void	readerror(int);
static void	readunits(const char *);
static int	reduceproduct(struct unittype *, int);
static int	reduceunit(struct unittype *);
static void	showanswer(struct unittype *, struct unittype *);
static void	showunit(struct unittype *);
static void	sortunit(struct unittype *);
__dead static void	usage(void);
static void	zeroerror(void);
static char   *dupstr(const char *);
static const char *lookupunit(const char *);

static char *
dupstr(const char *str)
{
	char *ret;

	ret = strdup(str);
	if (!ret)
		err(3, "Memory allocation error");
	return (ret);
}


static void
mywarnx(const char *fmt, ...)
{
	va_list args;

	va_start(args, fmt);
	if (errprefix) {
		/* warn to stdout, with errprefix prepended */
		printf("%s", errprefix);
		vprintf(fmt, args);
		printf("%s", "\n");
	} else {
		/* warn to stderr */
		vwarnx(fmt, args);
	}
	va_end(args);
}

static void
readerror(int linenum)
{
	mywarnx("Error in units file '%s' line %d", UNITSFILE, linenum);
}


static void
readunits(const char *userfile)
{
	FILE *unitfile;
	char line[80], *lineptr;
	int len, linenum, i, isdup;

	unitcount = 0;
	linenum = 0;

	if (userfile) {
		unitfile = fopen(userfile, "rt");
		if (!unitfile)
			err(1, "Unable to open units file '%s'", userfile);
	}
	else {
		unitfile = fopen(UNITSFILE, "rt");
		if (!unitfile) {
			char *direc, *env;
			char filename[1000];
			char separator[2];

			env = getenv("PATH");
			if (env) {
				if (strchr(env, ';'))
					strlcpy(separator, ";",
					    sizeof(separator));
				else
					strlcpy(separator, ":",
					    sizeof(separator));
				direc = strtok(env, separator);
				while (direc) {
					strlcpy(filename, "", sizeof(filename));
					strlcat(filename, direc,
					    sizeof(filename));
					strlcat(filename, "/",
					    sizeof(filename));
					strlcat(filename, UNITSFILE,
					    sizeof(filename));
					unitfile = fopen(filename, "rt");
					if (unitfile)
						break;
					direc = strtok(NULL, separator);
				}
			}
			if (!unitfile)
				errx(1, "Can't find units file '%s'",
				    UNITSFILE);
		}
	}
	while (!feof(unitfile)) {
		if (!fgets(line, 79, unitfile))
			break;
		linenum++;
		lineptr = line;
		if (*lineptr == '/')
			continue;
		lineptr += strspn(lineptr, " \n\t");
		len = strcspn(lineptr, " \n\t");
		lineptr[len] = 0;
		if (!strlen(lineptr))
			continue;
		if (lineptr[strlen(lineptr) - 1] == '-') { /* it's a prefix */
			if (prefixcount == MAXPREFIXES) {
				mywarnx(
			"Memory for prefixes exceeded in line %d",
					linenum);
				continue;
			}
			lineptr[strlen(lineptr) - 1] = 0;
			for (isdup = 0, i = 0; i < prefixcount; i++) {
				if (!strcmp(prefixtable[i].prefixname,
				    lineptr)) {
					isdup = 1;
					break;
				}
			}
			if (isdup) {
				mywarnx(
			"Redefinition of prefix '%s' on line %d ignored",
				    lineptr, linenum);
				continue;
			}
			prefixtable[prefixcount].prefixname = dupstr(lineptr);
			lineptr += len + 1;
			if (!strlen(lineptr)) {
				readerror(linenum);
				continue;
			}
			lineptr += strspn(lineptr, " \n\t");
			len = strcspn(lineptr, "\n\t");
			lineptr[len] = 0;
			prefixtable[prefixcount++].prefixval = dupstr(lineptr);
		}
		else {		/* it's not a prefix */
			if (unitcount == MAXUNITS) {
				mywarnx("Memory for units exceeded in line %d",
				    linenum);
				continue;
			}
			for (isdup = 0, i = 0; i < unitcount; i++) {
				if (!strcmp(unittable[i].uname, lineptr)) {
					isdup = 1;
					break;
				}
			}
			if (isdup) {
				mywarnx(
				"Redefinition of unit '%s' on line %d ignored",
				    lineptr, linenum);
				continue;
			}
			unittable[unitcount].uname = dupstr(lineptr);
			lineptr += len + 1;
			lineptr += strspn(lineptr, " \n\t");
			if (!strlen(lineptr)) {
				readerror(linenum);
				continue;
			}
			len = strcspn(lineptr, "\n\t");
			lineptr[len] = 0;
			unittable[unitcount++].uval = dupstr(lineptr);
		}
	}
	fclose(unitfile);
}

static void
initializeunit(struct unittype * theunit)
{
	theunit->factor = 1.0;
	theunit->numerator[0] = theunit->denominator[0] = NULL;
}

static int
addsubunit(const char *product[], const char *toadd)
{
	const char **ptr;

	for (ptr = product; *ptr && *ptr != NULLUNIT; ptr++);
	if (ptr >= product + MAXSUBUNITS) {
		mywarnx("Memory overflow in unit reduction");
		return 1;
	}
	if (!*ptr)
		*(ptr + 1) = 0;
	*ptr = dupstr(toadd);
	return 0;
}

static void
showunit(struct unittype * theunit)
{
	const char **ptr;
	int printedslash;
	int counter = 1;

	printf("\t%.*g", precision, theunit->factor);
	for (ptr = theunit->numerator; *ptr; ptr++) {
		if (ptr > theunit->numerator && **ptr &&
		    !strcmp(*ptr, *(ptr - 1)))
			counter++;
		else {
			if (counter > 1)
				printf("%s%d", powerstring, counter);
			if (**ptr)
				printf(" %s", *ptr);
			counter = 1;
		}
	}
	if (counter > 1)
		printf("%s%d", powerstring, counter);
	counter = 1;
	printedslash = 0;
	for (ptr = theunit->denominator; *ptr; ptr++) {
		if (ptr > theunit->denominator && **ptr &&
		    !strcmp(*ptr, *(ptr - 1)))
			counter++;
		else {
			if (counter > 1)
				printf("%s%d", powerstring, counter);
			if (**ptr) {
				if (!printedslash)
					printf(" /");
				printedslash = 1;
				printf(" %s", *ptr);
			}
			counter = 1;
		}
	}
	if (counter > 1)
		printf("%s%d", powerstring, counter);
	printf("\n");
}

static void
zeroerror(void)
{
	mywarnx("Unit reduces to zero");
}

/*
   Adds the specified string to the unit.
   Flip is 0 for adding normally, 1 for adding reciprocal.

   Returns 0 for successful addition, nonzero on error.
*/

static int
addunit(struct unittype * theunit, const char *toadd, int flip)
{
	char *scratch, *savescr;
	char *item;
	char *divider, *slash;
	int doingtop;

	savescr = scratch = dupstr(toadd);
	for (slash = scratch + 1; *slash; slash++)
		if (*slash == '-' &&
		    (tolower((unsigned char)*(slash - 1)) != 'e' ||
		    !strchr(".0123456789", *(slash + 1))))
			*slash = ' ';
	slash = strchr(scratch, '/');
	if (slash)
		*slash = 0;
	doingtop = 1;
	do {
		item = strtok(scratch, " *\t\n/");
		while (item) {
			if (strchr("0123456789.", *item)) {
				/* item starts with a number */
				char *endptr;
				double num;

				divider = strchr(item, '|');
				if (divider) {
					*divider = 0;
					num = strtod(item, &endptr);
					if (!num) {
						zeroerror();
						return 1;
					}
					if (endptr != divider) {
						/* "6foo|2" is an error */
						mywarnx("Junk before '|'");
						return 1;
					}
					if (doingtop ^ flip)
						theunit->factor *= num;
					else
						theunit->factor /= num;
					num = strtod(divider + 1, &endptr);
					if (!num) {
						zeroerror();
						return 1;
					}
					if (doingtop ^ flip)
						theunit->factor /= num;
					else
						theunit->factor *= num;
					if (*endptr) {
						/* "6|2foo" is like "6|2 foo" */
						item = endptr;
						continue;
					}
				}
				else {
					num = strtod(item, &endptr);
					if (!num) {
						zeroerror();
						return 1;
					}
					if (doingtop ^ flip)
						theunit->factor *= num;
					else
						theunit->factor /= num;
					if (*endptr) {
						/* "3foo" is like "3 foo" */
						item = endptr;
						continue;
					}
				}
			}
			else {	/* item is not a number */
				int repeat = 1;

				if (strchr("23456789",
				    item[strlen(item) - 1])) {
					repeat = item[strlen(item) - 1] - '0';
					item[strlen(item) - 1] = 0;
				}
				for (; repeat; repeat--)
					if (addsubunit(doingtop ^ flip ? theunit->numerator : theunit->denominator, item))
						return 1;
			}
			item = strtok(NULL, " *\t/\n");
		}
		doingtop--;
		if (slash) {
			scratch = slash + 1;
		}
		else
			doingtop--;
	} while (doingtop >= 0);
	free(savescr);
	return 0;
}

static int
compare(const void *item1, const void *item2)
{
	return strcmp(*(const char * const *) item1,
		      *(const char * const *) item2);
}

static void
sortunit(struct unittype * theunit)
{
	const char **ptr;
	int count;

	for (count = 0, ptr = theunit->numerator; *ptr; ptr++, count++);
	qsort(theunit->numerator, count, sizeof(char *), compare);
	for (count = 0, ptr = theunit->denominator; *ptr; ptr++, count++);
	qsort(theunit->denominator, count, sizeof(char *), compare);
}

static void
cancelunit(struct unittype * theunit)
{
	const char **den, **num;
	int comp;

	den = theunit->denominator;
	num = theunit->numerator;

	while (*num && *den) {
		comp = strcmp(*den, *num);
		if (!comp) {
/*      if (*den!=NULLUNIT) free(*den);
      if (*num!=NULLUNIT) free(*num);*/
			*den++ = NULLUNIT;
			*num++ = NULLUNIT;
		}
		else if (comp < 0)
			den++;
		else
			num++;
	}
}




/*
   Looks up the definition for the specified unit.
   Returns a pointer to the definition or a null pointer
   if the specified unit does not appear in the units table.
*/

static char buffer[100];	/* buffer for lookupunit answers with
				   prefixes */

static const char *
lookupunit(const char *unit)
{
	int i;
	char *copy;

	for (i = 0; i < unitcount; i++) {
		if (!strcmp(unittable[i].uname, unit))
			return unittable[i].uval;
	}

	if (unit[strlen(unit) - 1] == '^') {
		copy = dupstr(unit);
		copy[strlen(copy) - 1] = 0;
		for (i = 0; i < unitcount; i++) {
			if (!strcmp(unittable[i].uname, copy)) {
				strlcpy(buffer, copy, sizeof(buffer));
				free(copy);
				return buffer;
			}
		}
		free(copy);
	}
	if (unit[strlen(unit) - 1] == 's') {
		copy = dupstr(unit);
		copy[strlen(copy) - 1] = 0;
		for (i = 0; i < unitcount; i++) {
			if (!strcmp(unittable[i].uname, copy)) {
				strlcpy(buffer, copy, sizeof(buffer));
				free(copy);
				return buffer;
			}
		}
		if (copy[strlen(copy) - 1] == 'e') {
			copy[strlen(copy) - 1] = 0;
			for (i = 0; i < unitcount; i++) {
				if (!strcmp(unittable[i].uname, copy)) {
					strlcpy(buffer, copy, sizeof(buffer));
					free(copy);
					return buffer;
				}
			}
		}
		free(copy);
	}
	for (i = 0; i < prefixcount; i++) {
		if (!strncmp(prefixtable[i].prefixname, unit,
			strlen(prefixtable[i].prefixname))) {
			unit += strlen(prefixtable[i].prefixname);
			if (!strlen(unit) || lookupunit(unit)) {
				strlcpy(buffer, prefixtable[i].prefixval,
				    sizeof(buffer));
				strlcat(buffer, " ", sizeof(buffer));
				strlcat(buffer, unit, sizeof(buffer));
				return buffer;
			}
		}
	}
	return 0;
}



/*
   reduces a product of symbolic units to primitive units.
   The three low bits are used to return flags:

     bit 0 (1) set on if reductions were performed without error.
     bit 1 (2) set on if no reductions are performed.
     bit 2 (4) set on if an unknown unit is discovered.
*/


#define ERROR 4

static int
reduceproduct(struct unittype * theunit, int flip)
{

	const char *toadd;
	const char **product;
	int didsomething = 2;

	if (flip)
		product = theunit->denominator;
	else
		product = theunit->numerator;

	for (; *product; product++) {

		for (;;) {
			if (!strlen(*product))
				break;
			toadd = lookupunit(*product);
			if (!toadd) {
				mywarnx("Unknown unit '%s'", *product);
				return ERROR;
			}
			if (strchr(toadd, PRIMITIVECHAR))
				break;
			didsomething = 1;
			if (*product != NULLUNIT) {
				free(__UNCONST(*product));
				*product = NULLUNIT;
			}
			if (addunit(theunit, toadd, flip))
				return ERROR;
		}
	}
	return didsomething;
}


/*
   Reduces numerator and denominator of the specified unit.
   Returns 0 on success, or 1 on unknown unit error.
*/

static int
reduceunit(struct unittype * theunit)
{
	int ret;

	ret = 1;
	while (ret & 1) {
		ret = reduceproduct(theunit, 0) | reduceproduct(theunit, 1);
		if (ret & 4)
			return 1;
	}
	return 0;
}

static int
compareproducts(const char **one, const char **two)
{
	while (*one || *two) {
		if (!*one && *two != NULLUNIT)
			return 1;
		if (!*two && *one != NULLUNIT)
			return 1;
		if (*one == NULLUNIT)
			one++;
		else if (*two == NULLUNIT)
			two++;
		else if (*one && *two && strcmp(*one, *two))
			return 1;
		else
			one++, two++;
	}
	return 0;
}


/* Return zero if units are compatible, nonzero otherwise */

static int
compareunits(struct unittype * first, struct unittype * second)
{
	return
	compareproducts(first->numerator, second->numerator) ||
	compareproducts(first->denominator, second->denominator);
}

static int
compareunitsreciprocal(struct unittype * first, struct unittype * second)
{
	return
	compareproducts(first->numerator, second->denominator) ||
	compareproducts(first->denominator, second->numerator);
}


static int
completereduce(struct unittype * unit)
{
	if (reduceunit(unit))
		return 1;
	sortunit(unit);
	cancelunit(unit);
	return 0;
}


static void
showanswer(struct unittype * have, struct unittype * want)
{
	if (compareunits(have, want)) {
		if (compareunitsreciprocal(have, want)) {
			printf("conformability error\n");
			showunit(have);
			showunit(want);
		} else {
			printf("\treciprocal conversion\n");
			printf("\t* %.*g\n\t/ %.*g\n",
			    precision, 1 / (have->factor * want->factor),
			    precision, want->factor * have->factor);
		}
	}
	else
		printf("\t* %.*g\n\t/ %.*g\n",
		    precision, have->factor / want->factor,
		    precision, want->factor / have->factor);
}

static int
listunits(int expand)
{
	struct unittype theunit;
	const char *thename;
	const char *thedefn;
	int errors = 0;
	int i;
	int printexpansion;

	/*
	 * send error and warning messages to stdout,
	 * and make them look like comments.
	 */
	errprefix = "/ ";

#if 0 /* debug */
	printf("/ expand=%d precision=%d unitcount=%d prefixcount=%d\n",
	    expand, precision, unitcount, prefixcount);
#endif

	/* 1. Dump all primitive units, e.g. "m !a!", "kg !b!", ... */
	printf("/ Primitive units\n");
	for (i = 0; i < unitcount; i++) {
		thename = unittable[i].uname;
		thedefn = unittable[i].uval;
		if (thedefn[0] == PRIMITIVECHAR) {
			printf("%s\t%s\n", thename, thedefn);
		}
	}

	/* 2. Dump all prefixes, e.g. "yotta- 1e24", "zetta- 1e21", ... */
	printf("/ Prefixes\n");
	for (i = 0; i < prefixcount; i++) {
		printexpansion = expand;
		thename = prefixtable[i].prefixname;
		thedefn = prefixtable[i].prefixval;
		if (expand) {
			/*
			 * prefix names are sometimes identical to unit
			 * names, so we have to expand thedefn instead of
			 * expanding thename.
			 */
			initializeunit(&theunit);
			if (addunit(&theunit, thedefn, 0) != 0
			    || completereduce(&theunit) != 0) {
				errors++;
				printexpansion = 0;
				mywarnx("Error in prefix '%s-'", thename);
			}
		}
		if (printexpansion) {
			printf("%s-", thename);
			showunit(&theunit);
		} else
			printf("%s-\t%s\n", thename, thedefn);
	}

	/* 3. Dump all other units. */
	printf("/ Other units\n");
	for (i = 0; i < unitcount; i++) {
		printexpansion = expand;
		thename = unittable[i].uname;
		thedefn = unittable[i].uval;
		if (thedefn[0] == PRIMITIVECHAR)
			continue;
		if (expand) {
			/*
			 * expand thename, not thedefn, so that
			 * we can catch errors in the name itself.
			 * e.g. a name that contains a hyphen
			 * will be interpreted as multiplication.
			 */
			initializeunit(&theunit);
			if (addunit(&theunit, thename, 0) != 0
			    || completereduce(&theunit) != 0) {
				errors++;
				printexpansion = 0;
				mywarnx("Error in unit '%s'", thename);
			}
		}
		if (printexpansion) {
			printf("%s", thename);
			showunit(&theunit);
		} else
			printf("%s\t%s\n", thename, thedefn);
	}

	if (errors)
		mywarnx("Definitions with errors: %d", errors);
	return (errors ? 1 : 0);
}

static void
usage(void)
{
	fprintf(stderr,
	    "\nunits [-Llqv] [-f filename] [[count] from-unit to-unit]\n");
	fprintf(stderr, "\n    -f specify units file\n");
	fprintf(stderr, "    -L list units in standardized base units\n");
	fprintf(stderr, "    -l list units\n");
	fprintf(stderr, "    -q suppress prompting (quiet)\n");
	fprintf(stderr, "    -v print version number\n");
	exit(3);
}

int
main(int argc, char **argv)
{

	struct unittype have, want;
	char havestr[81], wantstr[81];
	int optchar;
	const char *userfile = 0;
	int list = 0, listexpand = 0;
	int quiet = 0;

	while ((optchar = getopt(argc, argv, "lLvqf:")) != -1) {
		switch (optchar) {
		case 'l':
			list = 1;
			break;
		case 'L':
			list = 1;
			listexpand = 1;
			precision = DBL_DIG;
			break;
		case 'f':
			userfile = optarg;
			break;
		case 'q':
			quiet = 1;
			break;
		case 'v':
			fprintf(stderr, "\n  units version %s  Copyright (c) 1993 by Adrian Mariano\n",
			    VERSION);
			fprintf(stderr, "                    This program may be freely distributed\n");
			usage();
		default:
			usage();
			break;
		}
	}

	argc -= optind;
	argv += optind;

	if ((argc != 3 && argc != 2 && argc != 0)
	    || (list && argc != 0))
		usage();

	if (list)
		errprefix = "/ ";	/* set this before reading the file */

	readunits(userfile);

	if (list)
		return listunits(listexpand);

	if (argc == 3) {
		strlcpy(havestr, argv[0], sizeof(havestr));
		strlcat(havestr, " ", sizeof(havestr));
		strlcat(havestr, argv[1], sizeof(havestr));
		argc--;
		argv++;
		argv[0] = havestr;
	}

	if (argc == 2) {
		strlcpy(havestr, argv[0], sizeof(havestr));
		strlcpy(wantstr, argv[1], sizeof(wantstr));
		initializeunit(&have);
		addunit(&have, havestr, 0);
		completereduce(&have);
		initializeunit(&want);
		addunit(&want, wantstr, 0);
		completereduce(&want);
		showanswer(&have, &want);
	}
	else {
		if (!quiet)
			printf("%d units, %d prefixes\n\n", unitcount,
			    prefixcount);
		for (;;) {
			do {
				initializeunit(&have);
				if (!quiet)
					printf("You have: ");
				if (!fgets(havestr, 80, stdin)) {
					if (!quiet)
						putchar('\n');
					exit(0);
				}
			} while (addunit(&have, havestr, 0) ||
			    completereduce(&have));
			do {
				initializeunit(&want);
				if (!quiet)
					printf("You want: ");
				if (!fgets(wantstr, 80, stdin)) {
					if (!quiet)
						putchar('\n');
					exit(0);
				}
			} while (addunit(&want, wantstr, 0) ||
			    completereduce(&want));
			showanswer(&have, &want);
		}
	}
	return (0);
}
