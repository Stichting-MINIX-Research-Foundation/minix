%{
/*
**  Originally written by Steven M. Bellovin <smb@research.att.com> while
**  at the University of North Carolina at Chapel Hill.  Later tweaked by
**  a couple of people on Usenet.  Completely overhauled by Rich $alz
**  <rsalz@bbn.com> and Jim Berets <jberets@bbn.com> in August, 1990;
**
**  This grammar has 10 shift/reduce conflicts.
**
**  This code is in the public domain and has no copyright.
*/
/* SUPPRESS 287 on yaccpar_sccsid *//* Unused static variable */
/* SUPPRESS 288 on yyerrlab *//* Label unused */

#include <sys/cdefs.h>
#ifdef __RCSID
__RCSID("$NetBSD: parsedate.y,v 1.20 2014/10/08 17:38:28 apb Exp $");
#endif

#include <stdio.h>
#include <ctype.h>
#include <errno.h>
#include <string.h>
#include <time.h>
#include <util.h>
#include <stdlib.h>

/* NOTES on rebuilding parsedate.c (particularly for inclusion in CVS
   releases):

   We don't want to mess with all the portability hassles of alloca.
   In particular, most (all?) versions of bison will use alloca in
   their parser.  If bison works on your system (e.g. it should work
   with gcc), then go ahead and use it, but the more general solution
   is to use byacc instead of bison, which should generate a portable
   parser.  I played with adding "#define alloca dont_use_alloca", to
   give an error if the parser generator uses alloca (and thus detect
   unportable parsedate.c's), but that seems to cause as many problems
   as it solves.  */

#define EPOCH		1970
#define HOUR(x)		((time_t)(x) * 60)
#define SECSPERDAY	(24L * 60L * 60L)

#define USE_LOCAL_TIME	99999 /* special case for Convert() and yyTimezone */

/*
**  An entry in the lexical lookup table.
*/
typedef struct _TABLE {
    const char	*name;
    int		type;
    time_t	value;
} TABLE;


/*
**  Daylight-savings mode:  on, off, or not yet known.
*/
typedef enum _DSTMODE {
    DSTon, DSToff, DSTmaybe
} DSTMODE;

/*
**  Meridian:  am, pm, or 24-hour style.
*/
typedef enum _MERIDIAN {
    MERam, MERpm, MER24
} MERIDIAN;


struct dateinfo {
	DSTMODE	yyDSTmode;	/* DST on/off/maybe */
	time_t	yyDayOrdinal;
	time_t	yyDayNumber;
	int	yyHaveDate;
	int	yyHaveFullYear;	/* if true, year is not abbreviated. */
				/* if false, need to call AdjustYear(). */
	int	yyHaveDay;
	int	yyHaveRel;
	int	yyHaveTime;
	int	yyHaveZone;
	time_t	yyTimezone;	/* Timezone as minutes ahead/east of UTC */
	time_t	yyDay;		/* Day of month [1-31] */
	time_t	yyHour;		/* Hour of day [0-24] or [1-12] */
	time_t	yyMinutes;	/* Minute of hour [0-59] */
	time_t	yyMonth;	/* Month of year [1-12] */
	time_t	yySeconds;	/* Second of minute [0-60] */
	time_t	yyYear;		/* Year, see also yyHaveFullYear */
	MERIDIAN yyMeridian;	/* Interpret yyHour as AM/PM/24 hour clock */
	time_t	yyRelMonth;
	time_t	yyRelSeconds;
};
%}

%union {
    time_t		Number;
    enum _MERIDIAN	Meridian;
}

%token	tAGO tDAY tDAYZONE tID tMERIDIAN tMINUTE_UNIT tMONTH tMONTH_UNIT
%token	tSEC_UNIT tSNUMBER tUNUMBER tZONE tDST AT_SIGN

%type	<Number>	tDAY tDAYZONE tMINUTE_UNIT tMONTH tMONTH_UNIT
%type	<Number>	tSEC_UNIT tSNUMBER tUNUMBER tZONE
%type	<Meridian>	tMERIDIAN o_merid

%parse-param	{ struct dateinfo *param }
%parse-param 	{ const char **yyInput }
%lex-param	{ const char **yyInput }
%pure-parser

%%

spec	: /* NULL */
	| spec item
	;

item	: time {
	    param->yyHaveTime++;
	}
	| time_numericzone {
	    param->yyHaveTime++;
	    param->yyHaveZone++;
	}
	| zone {
	    param->yyHaveZone++;
	}
	| date {
	    param->yyHaveDate++;
	}
	| day {
	    param->yyHaveDay++;
	}
	| rel {
	    param->yyHaveRel++;
	}
	| cvsstamp {
	    param->yyHaveTime++;
	    param->yyHaveDate++;
	    param->yyHaveZone++;
	}
	| epochdate {
	    param->yyHaveTime++;
	    param->yyHaveDate++;
	    param->yyHaveZone++;
	}
	| number
	;

cvsstamp: tUNUMBER '.' tUNUMBER '.' tUNUMBER '.' tUNUMBER '.' tUNUMBER '.' tUNUMBER {
	    param->yyYear = $1;
	    if (param->yyYear < 100) param->yyYear += 1900;
	    param->yyHaveFullYear = 1;
	    param->yyMonth = $3;
	    param->yyDay = $5;
	    param->yyHour = $7;
	    param->yyMinutes = $9;
	    param->yySeconds = $11;
	    param->yyDSTmode = DSToff;
	    param->yyTimezone = 0;
	}
	;

epochdate: AT_SIGN at_number {
            time_t    when = $<Number>2;
            struct tm tmbuf;
            if (gmtime_r(&when, &tmbuf) != NULL) {
		param->yyYear = tmbuf.tm_year + 1900;
		param->yyMonth = tmbuf.tm_mon + 1;
		param->yyDay = tmbuf.tm_mday;

		param->yyHour = tmbuf.tm_hour;
		param->yyMinutes = tmbuf.tm_min;
		param->yySeconds = tmbuf.tm_sec;
	    } else {
		param->yyYear = EPOCH;
		param->yyMonth = 1;
		param->yyDay = 1;

		param->yyHour = 0;
		param->yyMinutes = 0;
		param->yySeconds = 0;
	    }
	    param->yyHaveFullYear = 1;
	    param->yyDSTmode = DSToff;
	    param->yyTimezone = 0;
	}
	;

at_number : tUNUMBER | tSNUMBER ;

time	: tUNUMBER tMERIDIAN {
	    param->yyHour = $1;
	    param->yyMinutes = 0;
	    param->yySeconds = 0;
	    param->yyMeridian = $2;
	}
	| tUNUMBER ':' tUNUMBER o_merid {
	    param->yyHour = $1;
	    param->yyMinutes = $3;
	    param->yySeconds = 0;
	    param->yyMeridian = $4;
	}
	| tUNUMBER ':' tUNUMBER ':' tUNUMBER o_merid {
	    param->yyHour = $1;
	    param->yyMinutes = $3;
	    param->yySeconds = $5;
	    param->yyMeridian = $6;
	}
	| tUNUMBER ':' tUNUMBER ':' tUNUMBER '.' tUNUMBER {
	    param->yyHour = $1;
	    param->yyMinutes = $3;
	    param->yySeconds = $5;
	    param->yyMeridian = MER24;
/* XXX: Do nothing with millis */
	}
	;

time_numericzone : tUNUMBER ':' tUNUMBER tSNUMBER {
	    param->yyHour = $1;
	    param->yyMinutes = $3;
	    param->yyMeridian = MER24;
	    param->yyDSTmode = DSToff;
	    param->yyTimezone = - ($4 % 100 + ($4 / 100) * 60);
	}
	| tUNUMBER ':' tUNUMBER ':' tUNUMBER tSNUMBER {
	    param->yyHour = $1;
	    param->yyMinutes = $3;
	    param->yySeconds = $5;
	    param->yyMeridian = MER24;
	    param->yyDSTmode = DSToff;
	    param->yyTimezone = - ($6 % 100 + ($6 / 100) * 60);
	}
	;

zone	: tZONE {
	    param->yyTimezone = $1;
	    param->yyDSTmode = DSToff;
	}
	| tDAYZONE {
	    param->yyTimezone = $1;
	    param->yyDSTmode = DSTon;
	}
	|
	  tZONE tDST {
	    param->yyTimezone = $1;
	    param->yyDSTmode = DSTon;
	}
	;

day	: tDAY {
	    param->yyDayOrdinal = 1;
	    param->yyDayNumber = $1;
	}
	| tDAY ',' {
	    param->yyDayOrdinal = 1;
	    param->yyDayNumber = $1;
	}
	| tUNUMBER tDAY {
	    param->yyDayOrdinal = $1;
	    param->yyDayNumber = $2;
	}
	;

date	: tUNUMBER '/' tUNUMBER {
	    param->yyMonth = $1;
	    param->yyDay = $3;
	}
	| tUNUMBER '/' tUNUMBER '/' tUNUMBER {
	    if ($1 >= 100) {
		param->yyYear = $1;
		param->yyMonth = $3;
		param->yyDay = $5;
	    } else {
		param->yyMonth = $1;
		param->yyDay = $3;
		param->yyYear = $5;
	    }
	}
	| tUNUMBER tSNUMBER tSNUMBER {
	    /* ISO 8601 format.  yyyy-mm-dd.  */
	    param->yyYear = $1;
	    param->yyHaveFullYear = 1;
	    param->yyMonth = -$2;
	    param->yyDay = -$3;
	}
	| tUNUMBER tMONTH tSNUMBER {
	    /* e.g. 17-JUN-1992.  */
	    param->yyDay = $1;
	    param->yyMonth = $2;
	    param->yyYear = -$3;
	}
	| tMONTH tUNUMBER {
	    param->yyMonth = $1;
	    param->yyDay = $2;
	}
	| tMONTH tUNUMBER ',' tUNUMBER {
	    param->yyMonth = $1;
	    param->yyDay = $2;
	    param->yyYear = $4;
	}
	| tUNUMBER tMONTH {
	    param->yyMonth = $2;
	    param->yyDay = $1;
	}
	| tUNUMBER tMONTH tUNUMBER {
	    param->yyMonth = $2;
	    param->yyDay = $1;
	    param->yyYear = $3;
	}
	;

rel	: relunit tAGO {
	    param->yyRelSeconds = -param->yyRelSeconds;
	    param->yyRelMonth = -param->yyRelMonth;
	}
	| relunit
	;

relunit	: tUNUMBER tMINUTE_UNIT {
	    param->yyRelSeconds += $1 * $2 * 60L;
	}
	| tSNUMBER tMINUTE_UNIT {
	    param->yyRelSeconds += $1 * $2 * 60L;
	}
	| tMINUTE_UNIT {
	    param->yyRelSeconds += $1 * 60L;
	}
	| tSNUMBER tSEC_UNIT {
	    param->yyRelSeconds += $1;
	}
	| tUNUMBER tSEC_UNIT {
	    param->yyRelSeconds += $1;
	}
	| tSEC_UNIT {
	    param->yyRelSeconds++;
	}
	| tSNUMBER tMONTH_UNIT {
	    param->yyRelMonth += $1 * $2;
	}
	| tUNUMBER tMONTH_UNIT {
	    param->yyRelMonth += $1 * $2;
	}
	| tMONTH_UNIT {
	    param->yyRelMonth += $1;
	}
	;

number	: tUNUMBER {
	    if (param->yyHaveTime && param->yyHaveDate && !param->yyHaveRel)
		param->yyYear = $1;
	    else {
		if($1>10000) {
		    param->yyHaveDate++;
		    param->yyDay= ($1)%100;
		    param->yyMonth= ($1/100)%100;
		    param->yyYear = $1/10000;
		}
		else {
		    param->yyHaveTime++;
		    if ($1 < 100) {
			param->yyHour = $1;
			param->yyMinutes = 0;
		    }
		    else {
		    	param->yyHour = $1 / 100;
		    	param->yyMinutes = $1 % 100;
		    }
		    param->yySeconds = 0;
		    param->yyMeridian = MER24;
	        }
	    }
	}
	;

o_merid	: /* NULL */ {
	    $$ = MER24;
	}
	| tMERIDIAN {
	    $$ = $1;
	}
	;

%%

/* Month and day table. */
static const TABLE MonthDayTable[] = {
    { "january",	tMONTH,  1 },
    { "february",	tMONTH,  2 },
    { "march",		tMONTH,  3 },
    { "april",		tMONTH,  4 },
    { "may",		tMONTH,  5 },
    { "june",		tMONTH,  6 },
    { "july",		tMONTH,  7 },
    { "august",		tMONTH,  8 },
    { "september",	tMONTH,  9 },
    { "sept",		tMONTH,  9 },
    { "october",	tMONTH, 10 },
    { "november",	tMONTH, 11 },
    { "december",	tMONTH, 12 },
    { "sunday",		tDAY, 0 },
    { "monday",		tDAY, 1 },
    { "tuesday",	tDAY, 2 },
    { "tues",		tDAY, 2 },
    { "wednesday",	tDAY, 3 },
    { "wednes",		tDAY, 3 },
    { "thursday",	tDAY, 4 },
    { "thur",		tDAY, 4 },
    { "thurs",		tDAY, 4 },
    { "friday",		tDAY, 5 },
    { "saturday",	tDAY, 6 },
    { NULL,		0,    0 }
};

/* Time units table. */
static const TABLE UnitsTable[] = {
    { "year",		tMONTH_UNIT,	12 },
    { "month",		tMONTH_UNIT,	1 },
    { "fortnight",	tMINUTE_UNIT,	14 * 24 * 60 },
    { "week",		tMINUTE_UNIT,	7 * 24 * 60 },
    { "day",		tMINUTE_UNIT,	1 * 24 * 60 },
    { "hour",		tMINUTE_UNIT,	60 },
    { "minute",		tMINUTE_UNIT,	1 },
    { "min",		tMINUTE_UNIT,	1 },
    { "second",		tSEC_UNIT,	1 },
    { "sec",		tSEC_UNIT,	1 },
    { NULL,		0,		0 }
};

/* Assorted relative-time words. */
static const TABLE OtherTable[] = {
    { "tomorrow",	tMINUTE_UNIT,	1 * 24 * 60 },
    { "yesterday",	tMINUTE_UNIT,	-1 * 24 * 60 },
    { "today",		tMINUTE_UNIT,	0 },
    { "now",		tMINUTE_UNIT,	0 },
    { "last",		tUNUMBER,	-1 },
    { "this",		tMINUTE_UNIT,	0 },
    { "next",		tUNUMBER,	2 },
    { "first",		tUNUMBER,	1 },
    { "one",		tUNUMBER,	1 },
/*  { "second",		tUNUMBER,	2 }, */
    { "two",		tUNUMBER,	2 },
    { "third",		tUNUMBER,	3 },
    { "three",		tUNUMBER,	3 },
    { "fourth",		tUNUMBER,	4 },
    { "four",		tUNUMBER,	4 },
    { "fifth",		tUNUMBER,	5 },
    { "five",		tUNUMBER,	5 },
    { "sixth",		tUNUMBER,	6 },
    { "six",		tUNUMBER,	6 },
    { "seventh",	tUNUMBER,	7 },
    { "seven",		tUNUMBER,	7 },
    { "eighth",		tUNUMBER,	8 },
    { "eight",		tUNUMBER,	8 },
    { "ninth",		tUNUMBER,	9 },
    { "nine",		tUNUMBER,	9 },
    { "tenth",		tUNUMBER,	10 },
    { "ten",		tUNUMBER,	10 },
    { "eleventh",	tUNUMBER,	11 },
    { "eleven",		tUNUMBER,	11 },
    { "twelfth",	tUNUMBER,	12 },
    { "twelve",		tUNUMBER,	12 },
    { "ago",		tAGO,	1 },
    { NULL,		0,	0 }
};

/* The timezone table. */
/* Some of these are commented out because a time_t can't store a float. */
static const TABLE TimezoneTable[] = {
    { "gmt",	tZONE,     HOUR( 0) },	/* Greenwich Mean */
    { "ut",	tZONE,     HOUR( 0) },	/* Universal (Coordinated) */
    { "utc",	tZONE,     HOUR( 0) },
    { "wet",	tZONE,     HOUR( 0) },	/* Western European */
    { "bst",	tDAYZONE,  HOUR( 0) },	/* British Summer */
    { "wat",	tZONE,     HOUR( 1) },	/* West Africa */
    { "at",	tZONE,     HOUR( 2) },	/* Azores */
#if	0
    /* For completeness.  BST is also British Summer, and GST is
     * also Guam Standard. */
    { "bst",	tZONE,     HOUR( 3) },	/* Brazil Standard */
    { "gst",	tZONE,     HOUR( 3) },	/* Greenland Standard */
#endif
#if 0
    { "nft",	tZONE,     HOUR(3.5) },	/* Newfoundland */
    { "nst",	tZONE,     HOUR(3.5) },	/* Newfoundland Standard */
    { "ndt",	tDAYZONE,  HOUR(3.5) },	/* Newfoundland Daylight */
#endif
    { "ast",	tZONE,     HOUR( 4) },	/* Atlantic Standard */
    { "adt",	tDAYZONE,  HOUR( 4) },	/* Atlantic Daylight */
    { "est",	tZONE,     HOUR( 5) },	/* Eastern Standard */
    { "edt",	tDAYZONE,  HOUR( 5) },	/* Eastern Daylight */
    { "cst",	tZONE,     HOUR( 6) },	/* Central Standard */
    { "cdt",	tDAYZONE,  HOUR( 6) },	/* Central Daylight */
    { "mst",	tZONE,     HOUR( 7) },	/* Mountain Standard */
    { "mdt",	tDAYZONE,  HOUR( 7) },	/* Mountain Daylight */
    { "pst",	tZONE,     HOUR( 8) },	/* Pacific Standard */
    { "pdt",	tDAYZONE,  HOUR( 8) },	/* Pacific Daylight */
    { "yst",	tZONE,     HOUR( 9) },	/* Yukon Standard */
    { "ydt",	tDAYZONE,  HOUR( 9) },	/* Yukon Daylight */
    { "hst",	tZONE,     HOUR(10) },	/* Hawaii Standard */
    { "hdt",	tDAYZONE,  HOUR(10) },	/* Hawaii Daylight */
    { "cat",	tZONE,     HOUR(10) },	/* Central Alaska */
    { "ahst",	tZONE,     HOUR(10) },	/* Alaska-Hawaii Standard */
    { "nt",	tZONE,     HOUR(11) },	/* Nome */
    { "idlw",	tZONE,     HOUR(12) },	/* International Date Line West */
    { "cet",	tZONE,     -HOUR(1) },	/* Central European */
    { "met",	tZONE,     -HOUR(1) },	/* Middle European */
    { "mewt",	tZONE,     -HOUR(1) },	/* Middle European Winter */
    { "mest",	tDAYZONE,  -HOUR(1) },	/* Middle European Summer */
    { "swt",	tZONE,     -HOUR(1) },	/* Swedish Winter */
    { "sst",	tDAYZONE,  -HOUR(1) },	/* Swedish Summer */
    { "fwt",	tZONE,     -HOUR(1) },	/* French Winter */
    { "fst",	tDAYZONE,  -HOUR(1) },	/* French Summer */
    { "eet",	tZONE,     -HOUR(2) },	/* Eastern Europe, USSR Zone 1 */
    { "bt",	tZONE,     -HOUR(3) },	/* Baghdad, USSR Zone 2 */
#if 0
    { "it",	tZONE,     -HOUR(3.5) },/* Iran */
#endif
    { "zp4",	tZONE,     -HOUR(4) },	/* USSR Zone 3 */
    { "zp5",	tZONE,     -HOUR(5) },	/* USSR Zone 4 */
#if 0
    { "ist",	tZONE,     -HOUR(5.5) },/* Indian Standard */
#endif
    { "zp6",	tZONE,     -HOUR(6) },	/* USSR Zone 5 */
#if	0
    /* For completeness.  NST is also Newfoundland Stanard, and SST is
     * also Swedish Summer. */
    { "nst",	tZONE,     -HOUR(6.5) },/* North Sumatra */
    { "sst",	tZONE,     -HOUR(7) },	/* South Sumatra, USSR Zone 6 */
#endif	/* 0 */
    { "wast",	tZONE,     -HOUR(7) },	/* West Australian Standard */
    { "wadt",	tDAYZONE,  -HOUR(7) },	/* West Australian Daylight */
#if 0
    { "jt",	tZONE,     -HOUR(7.5) },/* Java (3pm in Cronusland!) */
#endif
    { "cct",	tZONE,     -HOUR(8) },	/* China Coast, USSR Zone 7 */
    { "jst",	tZONE,     -HOUR(9) },	/* Japan Standard, USSR Zone 8 */
#if 0
    { "cast",	tZONE,     -HOUR(9.5) },/* Central Australian Standard */
    { "cadt",	tDAYZONE,  -HOUR(9.5) },/* Central Australian Daylight */
#endif
    { "east",	tZONE,     -HOUR(10) },	/* Eastern Australian Standard */
    { "eadt",	tDAYZONE,  -HOUR(10) },	/* Eastern Australian Daylight */
    { "gst",	tZONE,     -HOUR(10) },	/* Guam Standard, USSR Zone 9 */
    { "nzt",	tZONE,     -HOUR(12) },	/* New Zealand */
    { "nzst",	tZONE,     -HOUR(12) },	/* New Zealand Standard */
    { "nzdt",	tDAYZONE,  -HOUR(12) },	/* New Zealand Daylight */
    { "idle",	tZONE,     -HOUR(12) },	/* International Date Line East */
    {  NULL,	0,	    0 }
};

/* Military timezone table. */
static const TABLE MilitaryTable[] = {
    { "a",	tZONE,	HOUR(  1) },
    { "b",	tZONE,	HOUR(  2) },
    { "c",	tZONE,	HOUR(  3) },
    { "d",	tZONE,	HOUR(  4) },
    { "e",	tZONE,	HOUR(  5) },
    { "f",	tZONE,	HOUR(  6) },
    { "g",	tZONE,	HOUR(  7) },
    { "h",	tZONE,	HOUR(  8) },
    { "i",	tZONE,	HOUR(  9) },
    { "k",	tZONE,	HOUR( 10) },
    { "l",	tZONE,	HOUR( 11) },
    { "m",	tZONE,	HOUR( 12) },
    { "n",	tZONE,	HOUR(- 1) },
    { "o",	tZONE,	HOUR(- 2) },
    { "p",	tZONE,	HOUR(- 3) },
    { "q",	tZONE,	HOUR(- 4) },
    { "r",	tZONE,	HOUR(- 5) },
    { "s",	tZONE,	HOUR(- 6) },
    { "t",	tZONE,	HOUR(- 7) },
    { "u",	tZONE,	HOUR(- 8) },
    { "v",	tZONE,	HOUR(- 9) },
    { "w",	tZONE,	HOUR(-10) },
    { "x",	tZONE,	HOUR(-11) },
    { "y",	tZONE,	HOUR(-12) },
    { "z",	tZONE,	HOUR(  0) },
    { NULL,	0,	0 }
};




/* ARGSUSED */
static int
yyerror(struct dateinfo *param, const char **inp, const char *s __unused)
{
  return 0;
}


/* Adjust year from a value that might be abbreviated, to a full value.
 * e.g. convert 70 to 1970.
 * Input Year is either:
 *  - A negative number, which means to use its absolute value (why?)
 *  - A number from 0 to 99, which means a year from 1900 to 1999, or
 *  - The actual year (>=100).
 * Returns the full year. */
static time_t
AdjustYear(time_t Year)
{
    /* XXX Y2K */
    if (Year < 0)
	Year = -Year;
    if (Year < 70)
	Year += 2000;
    else if (Year < 100)
	Year += 1900;
    return Year;
}

static time_t
Convert(
    time_t	Month,		/* month of year [1-12] */
    time_t	Day,		/* day of month [1-31] */
    time_t	Year,		/* year, not abbreviated in any way */
    time_t	Hours,		/* Hour of day [0-24] */
    time_t	Minutes,	/* Minute of hour [0-59] */
    time_t	Seconds,	/* Second of minute [0-60] */
    time_t	Timezone,	/* Timezone as minutes east of UTC,
				 * or USE_LOCAL_TIME special case */
    MERIDIAN	Meridian,	/* Hours are am/pm/24 hour clock */
    DSTMODE	DSTmode		/* DST on/off/maybe */
)
{
    struct tm tm = {.tm_sec = 0};
    time_t result;

    tm.tm_sec = Seconds;
    tm.tm_min = Minutes;
    tm.tm_hour = Hours + (Meridian == MERpm ? 12 : 0);
    tm.tm_mday = Day;
    tm.tm_mon = Month - 1;
    tm.tm_year = Year - 1900;
    switch (DSTmode) {
    case DSTon:  tm.tm_isdst = 1; break;
    case DSToff: tm.tm_isdst = 0; break;
    default:     tm.tm_isdst = -1; break;
    }

    if (Timezone == USE_LOCAL_TIME) {
	    result = mktime(&tm);
    } else {
	    /* We rely on mktime_z(NULL, ...) working in UTC */
	    result = mktime_z(NULL, &tm);
	    result += Timezone * 60;
    }

#if PARSEDATE_DEBUG
    fprintf(stderr, "%s(M=%jd D=%jd Y=%jd H=%jd M=%jd S=%jd Z=%jd"
		    " mer=%d DST=%d)",
	__func__,
	(intmax_t)Month, (intmax_t)Day, (intmax_t)Year,
	(intmax_t)Hours, (intmax_t)Minutes, (intmax_t)Seconds,
	(intmax_t)Timezone, (int)Meridian, (int)DSTmode);
    fprintf(stderr, " -> %jd", (intmax_t)result);
    fprintf(stderr, " %s", ctime(&result));
#endif

    return result;
}


static time_t
DSTcorrect(
    time_t	Start,
    time_t	Future
)
{
    time_t	StartDay;
    time_t	FutureDay;
    struct tm  *tm;

    if ((tm = localtime(&Start)) == NULL)
	return -1;
    StartDay = (tm->tm_hour + 1) % 24;

    if ((tm = localtime(&Future)) == NULL)
	return -1;
    FutureDay = (tm->tm_hour + 1) % 24;

    return (Future - Start) + (StartDay - FutureDay) * 60L * 60L;
}


static time_t
RelativeDate(
    time_t	Start,
    time_t	DayOrdinal,
    time_t	DayNumber
)
{
    struct tm	*tm;
    time_t	now;

    now = Start;
    tm = localtime(&now);
    now += SECSPERDAY * ((DayNumber - tm->tm_wday + 7) % 7);
    now += 7 * SECSPERDAY * (DayOrdinal <= 0 ? DayOrdinal : DayOrdinal - 1);
    return DSTcorrect(Start, now);
}


static time_t
RelativeMonth(
    time_t	Start,
    time_t	RelMonth,
    time_t	Timezone
)
{
    struct tm	*tm;
    time_t	Month;
    time_t	Year;

    if (RelMonth == 0)
	return 0;
    tm = localtime(&Start);
    if (tm == NULL)
	return -1;
    Month = 12 * (tm->tm_year + 1900) + tm->tm_mon + RelMonth;
    Year = Month / 12;
    Month = Month % 12 + 1;
    return DSTcorrect(Start,
	    Convert(Month, (time_t)tm->tm_mday, Year,
		(time_t)tm->tm_hour, (time_t)tm->tm_min, (time_t)tm->tm_sec,
		Timezone, MER24, DSTmaybe));
}


static int
LookupWord(YYSTYPE *yylval, char *buff)
{
    register char	*p;
    register char	*q;
    register const TABLE	*tp;
    int			i;
    int			abbrev;

    /* Make it lowercase. */
    for (p = buff; *p; p++)
	if (isupper((unsigned char)*p))
	    *p = tolower((unsigned char)*p);

    if (strcmp(buff, "am") == 0 || strcmp(buff, "a.m.") == 0) {
	yylval->Meridian = MERam;
	return tMERIDIAN;
    }
    if (strcmp(buff, "pm") == 0 || strcmp(buff, "p.m.") == 0) {
	yylval->Meridian = MERpm;
	return tMERIDIAN;
    }

    /* See if we have an abbreviation for a month. */
    if (strlen(buff) == 3)
	abbrev = 1;
    else if (strlen(buff) == 4 && buff[3] == '.') {
	abbrev = 1;
	buff[3] = '\0';
    }
    else
	abbrev = 0;

    for (tp = MonthDayTable; tp->name; tp++) {
	if (abbrev) {
	    if (strncmp(buff, tp->name, 3) == 0) {
		yylval->Number = tp->value;
		return tp->type;
	    }
	}
	else if (strcmp(buff, tp->name) == 0) {
	    yylval->Number = tp->value;
	    return tp->type;
	}
    }

    for (tp = TimezoneTable; tp->name; tp++)
	if (strcmp(buff, tp->name) == 0) {
	    yylval->Number = tp->value;
	    return tp->type;
	}

    if (strcmp(buff, "dst") == 0) 
	return tDST;

    for (tp = UnitsTable; tp->name; tp++)
	if (strcmp(buff, tp->name) == 0) {
	    yylval->Number = tp->value;
	    return tp->type;
	}

    /* Strip off any plural and try the units table again. */
    i = strlen(buff) - 1;
    if (buff[i] == 's') {
	buff[i] = '\0';
	for (tp = UnitsTable; tp->name; tp++)
	    if (strcmp(buff, tp->name) == 0) {
		yylval->Number = tp->value;
		return tp->type;
	    }
	buff[i] = 's';		/* Put back for "this" in OtherTable. */
    }

    for (tp = OtherTable; tp->name; tp++)
	if (strcmp(buff, tp->name) == 0) {
	    yylval->Number = tp->value;
	    return tp->type;
	}

    /* Military timezones. */
    if (buff[1] == '\0' && isalpha((unsigned char)*buff)) {
	for (tp = MilitaryTable; tp->name; tp++)
	    if (strcmp(buff, tp->name) == 0) {
		yylval->Number = tp->value;
		return tp->type;
	    }
    }

    /* Drop out any periods and try the timezone table again. */
    for (i = 0, p = q = buff; *q; q++)
	if (*q != '.')
	    *p++ = *q;
	else
	    i++;
    *p = '\0';
    if (i)
	for (tp = TimezoneTable; tp->name; tp++)
	    if (strcmp(buff, tp->name) == 0) {
		yylval->Number = tp->value;
		return tp->type;
	    }

    return tID;
}


static int
yylex(YYSTYPE *yylval, const char **yyInput)
{
    register char	c;
    register char	*p;
    char		buff[20];
    int			Count;
    int			sign;
    const char		*inp = *yyInput;

    for ( ; ; ) {
	while (isspace((unsigned char)*inp))
	    inp++;

	if (isdigit((unsigned char)(c = *inp)) || c == '-' || c == '+') {
	    if (c == '-' || c == '+') {
		sign = c == '-' ? -1 : 1;
		if (!isdigit((unsigned char)*++inp))
		    /* skip the '-' sign */
		    continue;
	    }
	    else
		sign = 0;
	    for (yylval->Number = 0; isdigit((unsigned char)(c = *inp++)); )
		yylval->Number = 10 * yylval->Number + c - '0';
	    if (sign < 0)
		yylval->Number = -yylval->Number;
	    *yyInput = --inp;
	    return sign ? tSNUMBER : tUNUMBER;
	}
	if (isalpha((unsigned char)c)) {
	    for (p = buff; isalpha((unsigned char)(c = *inp++)) || c == '.'; )
		if (p < &buff[sizeof buff - 1])
		    *p++ = c;
	    *p = '\0';
	    *yyInput = --inp;
	    return LookupWord(yylval, buff);
	}
	if (c == '@') {
	    *yyInput = ++inp;
	    return AT_SIGN;
	}
	if (c != '(') {
	    *yyInput = ++inp;
	    return c;
	}
	Count = 0;
	do {
	    c = *inp++;
	    if (c == '\0')
		return c;
	    if (c == '(')
		Count++;
	    else if (c == ')')
		Count--;
	} while (Count > 0);
    }
}

#define TM_YEAR_ORIGIN 1900

time_t
parsedate(const char *p, const time_t *now, const int *zone)
{
    struct tm		local, *tm;
    time_t		nowt;
    int			zonet;
    time_t		Start;
    time_t		tod, rm;
    struct dateinfo	param;
    int			saved_errno;
    
    saved_errno = errno;
    errno = 0;

    if (now == NULL) {
        now = &nowt;
	(void)time(&nowt);
    }
    if (zone == NULL) {
	zone = &zonet;
	zonet = USE_LOCAL_TIME;
	if ((tm = localtime_r(now, &local)) == NULL)
	    return -1;
    } else {
	/*
	 * Should use the specified zone, not localtime.
	 * Fake it using gmtime and arithmetic.
	 * This is good enough because we use only the year/month/day,
	 * not other fields of struct tm.
	 */
	time_t fake = *now + (*zone * 60);
	if ((tm = gmtime_r(&fake, &local)) == NULL)
	    return -1;
    }
    param.yyYear = tm->tm_year + 1900;
    param.yyMonth = tm->tm_mon + 1;
    param.yyDay = tm->tm_mday;
    param.yyTimezone = *zone;
    param.yyDSTmode = DSTmaybe;
    param.yyHour = 0;
    param.yyMinutes = 0;
    param.yySeconds = 0;
    param.yyMeridian = MER24;
    param.yyRelSeconds = 0;
    param.yyRelMonth = 0;
    param.yyHaveDate = 0;
    param.yyHaveFullYear = 0;
    param.yyHaveDay = 0;
    param.yyHaveRel = 0;
    param.yyHaveTime = 0;
    param.yyHaveZone = 0;

    if (yyparse(&param, &p) || param.yyHaveTime > 1 || param.yyHaveZone > 1 ||
	param.yyHaveDate > 1 || param.yyHaveDay > 1) {
	errno = EINVAL;
	return -1;
    }

    if (param.yyHaveDate || param.yyHaveTime || param.yyHaveDay) {
	if (! param.yyHaveFullYear) {
		param.yyYear = AdjustYear(param.yyYear);
		param.yyHaveFullYear = 1;
	}
	Start = Convert(param.yyMonth, param.yyDay, param.yyYear, param.yyHour,
	    param.yyMinutes, param.yySeconds, param.yyTimezone,
	    param.yyMeridian, param.yyDSTmode);
	if (Start == -1 && errno != 0)
	    return -1;
    }
    else {
	Start = *now;
	if (!param.yyHaveRel)
	    Start -= ((tm->tm_hour * 60L + tm->tm_min) * 60L) + tm->tm_sec;
    }

    Start += param.yyRelSeconds;
    rm = RelativeMonth(Start, param.yyRelMonth, param.yyTimezone);
    if (rm == -1 && errno != 0)
	return -1;
    Start += rm;

    if (param.yyHaveDay && !param.yyHaveDate) {
	tod = RelativeDate(Start, param.yyDayOrdinal, param.yyDayNumber);
	Start += tod;
    }

    if (errno == 0)
	errno = saved_errno;
    return Start;
}


#if	defined(TEST)

/* ARGSUSED */
int
main(int ac, char *av[])
{
    char	buff[128];
    time_t	d;

    (void)printf("Enter date, or blank line to exit.\n\t> ");
    (void)fflush(stdout);
    while (fgets(buff, sizeof(buff), stdin) && buff[0] != '\n') {
	errno = 0;
	d = parsedate(buff, NULL, NULL);
	if (d == -1 && errno != 0)
	    (void)printf("Bad format - couldn't convert: %s\n",
	        strerror(errno));
	else
	    (void)printf("%jd\t%s", (intmax_t)d, ctime(&d));
	(void)printf("\t> ");
	(void)fflush(stdout);
    }
    exit(0);
    /* NOTREACHED */
}
#endif	/* defined(TEST) */
