/*	$NetBSD: gen_subs.c,v 1.36 2012/08/09 08:09:21 christos Exp $	*/

/*-
 * Copyright (c) 1992 Keith Muller.
 * Copyright (c) 1992, 1993
 *	The Regents of the University of California.  All rights reserved.
 *
 * This code is derived from software contributed to Berkeley by
 * Keith Muller of the University of California, San Diego.
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
#if !defined(lint)
#if 0
static char sccsid[] = "@(#)gen_subs.c	8.1 (Berkeley) 5/31/93";
#else
__RCSID("$NetBSD: gen_subs.c,v 1.36 2012/08/09 08:09:21 christos Exp $");
#endif
#endif /* not lint */

#include <sys/types.h>
#include <sys/time.h>
#include <sys/stat.h>
#include <sys/param.h>

#include <ctype.h>
#include <grp.h>
#include <pwd.h>
#include <vis.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <tzfile.h>
#include <unistd.h>

#include "pax.h"
#include "extern.h"

/*
 * a collection of general purpose subroutines used by pax
 */

/*
 * constants used by ls_list() when printing out archive members
 */
#define MODELEN 20
#define DATELEN 64
#define SIXMONTHS	 ((DAYSPERNYEAR / 2) * SECSPERDAY)
#define CURFRMT		"%b %e %H:%M"
#define OLDFRMT		"%b %e  %Y"
#ifndef UT_NAMESIZE
#define UT_NAMESIZE	8
#endif
#define UT_GRPSIZE	6

/*
 * convert time to string
 */
static void
formattime(char *buf, size_t buflen, time_t when)
{
	int error;
	struct tm tm;
	(void)localtime_r(&when, &tm);

	if (when + SIXMONTHS <= time(NULL))
		error = strftime(buf, buflen, OLDFRMT, &tm);
	else
		error = strftime(buf, buflen, CURFRMT, &tm);

	if (error == 0)
		buf[0] = '\0';
}

/*
 * ls_list()
 *	list the members of an archive in ls format
 */

void
ls_list(ARCHD *arcn, time_t now, FILE *fp)
{
	struct stat *sbp;
	char f_mode[MODELEN];
	char f_date[DATELEN];
	const char *user, *group;

	/*
	 * if not verbose, just print the file name
	 */
	if (!vflag) {
		(void)fprintf(fp, "%s\n", arcn->name);
		(void)fflush(fp);
		return;
	}

	/*
	 * user wants long mode
	 */
	sbp = &(arcn->sb);
	strmode(sbp->st_mode, f_mode);

	/*
	 * time format based on age compared to the time pax was started.
	 */
	formattime(f_date, sizeof(f_date), arcn->sb.st_mtime);
	/*
	 * print file mode, link count, uid, gid and time
	 */
	user = user_from_uid(sbp->st_uid, 0);
	group = group_from_gid(sbp->st_gid, 0);
	(void)fprintf(fp, "%s%2lu %-*s %-*s ", f_mode,
	    (unsigned long)sbp->st_nlink,
	    UT_NAMESIZE, user ? user : "", UT_GRPSIZE, group ? group : "");

	/*
	 * print device id's for devices, or sizes for other nodes
	 */
	if ((arcn->type == PAX_CHR) || (arcn->type == PAX_BLK))
		(void)fprintf(fp, "%4lu,%4lu ", (long) MAJOR(sbp->st_rdev),
		    (long) MINOR(sbp->st_rdev));
	else {
		(void)fprintf(fp, OFFT_FP("9") " ", (OFFT_T)sbp->st_size);
	}

	/*
	 * print name and link info for hard and soft links
	 */
	(void)fprintf(fp, "%s %s", f_date, arcn->name);
	if ((arcn->type == PAX_HLK) || (arcn->type == PAX_HRG))
		(void)fprintf(fp, " == %s\n", arcn->ln_name);
	else if (arcn->type == PAX_SLK)
		(void)fprintf(fp, " -> %s\n", arcn->ln_name);
	else
		(void)fputc('\n', fp);
	(void)fflush(fp);
}

/*
 * tty_ls()
 *	print a short summary of file to tty.
 */

void
ls_tty(ARCHD *arcn)
{
	char f_date[DATELEN];
	char f_mode[MODELEN];

	formattime(f_date, sizeof(f_date), arcn->sb.st_mtime);
	strmode(arcn->sb.st_mode, f_mode);
	tty_prnt("%s%s %s\n", f_mode, f_date, arcn->name);
	return;
}

void
safe_print(const char *str, FILE *fp)
{
	char visbuf[5];
	const char *cp;

	/*
	 * if printing to a tty, use vis(3) to print special characters.
	 */
	if (isatty(fileno(fp))) {
		for (cp = str; *cp; cp++) {
			(void)vis(visbuf, cp[0], VIS_CSTYLE, cp[1]);
			(void)fputs(visbuf, fp);
		}
	} else {
		(void)fputs(str, fp);
	}
}

/*
 * asc_u32()
 *	convert hex/octal character string into a uint32_t. We do not have to
 *	check for overflow! (the headers in all supported formats are not large
 *	enough to create an overflow).
 *	NOTE: strings passed to us are NOT TERMINATED.
 * Return:
 *	uint32_t value
 */

uint32_t
asc_u32(char *str, int len, int base)
{
	char *stop;
	uint32_t tval = 0;

	stop = str + len;

	/*
	 * skip over leading blanks and zeros
	 */
	while ((str < stop) && ((*str == ' ') || (*str == '0')))
		++str;

	/*
	 * for each valid digit, shift running value (tval) over to next digit
	 * and add next digit
	 */
	if (base == HEX) {
		while (str < stop) {
			if ((*str >= '0') && (*str <= '9'))
				tval = (tval << 4) + (*str++ - '0');
			else if ((*str >= 'A') && (*str <= 'F'))
				tval = (tval << 4) + 10 + (*str++ - 'A');
			else if ((*str >= 'a') && (*str <= 'f'))
				tval = (tval << 4) + 10 + (*str++ - 'a');
			else
				break;
		}
	} else {
		while ((str < stop) && (*str >= '0') && (*str <= '7'))
			tval = (tval << 3) + (*str++ - '0');
	}
	return tval;
}

/*
 * u32_asc()
 *	convert an uintmax_t into an hex/oct ascii string. pads with LEADING
 *	ascii 0's to fill string completely
 *	NOTE: the string created is NOT TERMINATED.
 */

int
u32_asc(uintmax_t val, char *str, int len, int base)
{
	char *pt;
	uint32_t digit;
	uintmax_t p;

	p = val & TOP_HALF;
	if (p && p != TOP_HALF)
		return -1;

	val &= BOTTOM_HALF;

	/*
	 * WARNING str is not '\0' terminated by this routine
	 */
	pt = str + len - 1;

	/*
	 * do a tailwise conversion (start at right most end of string to place
	 * least significant digit). Keep shifting until conversion value goes
	 * to zero (all digits were converted)
	 */
	if (base == HEX) {
		while (pt >= str) {
			if ((digit = (val & 0xf)) < 10)
				*pt-- = '0' + (char)digit;
			else
				*pt-- = 'a' + (char)(digit - 10);
			if ((val = (val >> 4)) == (u_long)0)
				break;
		}
	} else {
		while (pt >= str) {
			*pt-- = '0' + (char)(val & 0x7);
			if ((val = (val >> 3)) == 0)
				break;
		}
	}

	/*
	 * pad with leading ascii ZEROS. We return -1 if we ran out of space.
	 */
	while (pt >= str)
		*pt-- = '0';
	if (val != 0)
		return -1;
	return 0;
}

/*
 * asc_umax()
 *	convert hex/octal character string into a uintmax. We do
 *	not have to to check for overflow! (the headers in all supported
 *	formats are not large enough to create an overflow).
 *	NOTE: strings passed to us are NOT TERMINATED.
 * Return:
 *	uintmax_t value
 */

uintmax_t
asc_umax(char *str, int len, int base)
{
	char *stop;
	uintmax_t tval = 0;

	stop = str + len;

	/*
	 * skip over leading blanks and zeros
	 */
	while ((str < stop) && ((*str == ' ') || (*str == '0')))
		++str;

	/*
	 * for each valid digit, shift running value (tval) over to next digit
	 * and add next digit
	 */
	if (base == HEX) {
		while (str < stop) {
			if ((*str >= '0') && (*str <= '9'))
				tval = (tval << 4) + (*str++ - '0');
			else if ((*str >= 'A') && (*str <= 'F'))
				tval = (tval << 4) + 10 + (*str++ - 'A');
			else if ((*str >= 'a') && (*str <= 'f'))
				tval = (tval << 4) + 10 + (*str++ - 'a');
			else
				break;
		}
	} else {
		while ((str < stop) && (*str >= '0') && (*str <= '7'))
			tval = (tval << 3) + (*str++ - '0');
	}
	return tval;
}

/*
 * umax_asc()
 *	convert an uintmax_t into a hex/oct ascii string. pads with
 *	LEADING ascii 0's to fill string completely
 *	NOTE: the string created is NOT TERMINATED.
 */

int
umax_asc(uintmax_t val, char *str, int len, int base)
{
	char *pt;
	uintmax_t digit;

	/*
	 * WARNING str is not '\0' terminated by this routine
	 */
	pt = str + len - 1;

	/*
	 * do a tailwise conversion (start at right most end of string to place
	 * least significant digit). Keep shifting until conversion value goes
	 * to zero (all digits were converted)
	 */
	if (base == HEX) {
		while (pt >= str) {
			if ((digit = (val & 0xf)) < 10)
				*pt-- = '0' + (char)digit;
			else
				*pt-- = 'a' + (char)(digit - 10);
			if ((val = (val >> 4)) == 0)
				break;
		}
	} else {
		while (pt >= str) {
			*pt-- = '0' + (char)(val & 0x7);
			if ((val = (val >> 3)) == 0)
				break;
		}
	}

	/*
	 * pad with leading ascii ZEROS. We return -1 if we ran out of space.
	 */
	while (pt >= str)
		*pt-- = '0';
	if (val != 0)
		return -1;
	return 0;
}

int
check_Aflag(void)
{

	if (Aflag > 0)
		return 1;
	if (Aflag == 0) {
		Aflag = -1;
		tty_warn(0,
		 "Removing leading / from absolute path names in the archive");
	}
	return 0;
}
