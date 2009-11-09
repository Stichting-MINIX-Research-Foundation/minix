/* ELLE - Copyright 1984, 1987 by Ken Harrenstien, SRI International
 *	This software is quasi-public; it may be used freely with
 *	like software, but may NOT be sold or made part of licensed
 *	products without permission of the author.
 */
/*
 * EESITE.H	Site-dependent switches & definitions
 */

/* CONDITIONAL COMPILATION SWITCHES */

#define V6	0	/* Running on V6 system (else V7 assumed) */

#define APOLLO 0	/* Running on an Apollo system */
#define BBN	0	/* Running on BBN system (tty stuff) */
#define BSD4_2	0	/* Running on 4.2BSD system */
#define COHERENT 0	/* Running on Coherent IBM-PC system */
#define DNTTY	0	/* Running on SRI V6 Deafnet system (tty stuff) */
#define HPUX	0	/* Running on Hewlett-Packard System V + */
#define MINIX	1	/* Running on MINIX (IBM-PC) system */
#define ONYX	0	/* Running on ONYX Z8000 system */
#define PCIX	0	/* Running on PC/IX (IBM-PC) system */
#define SUN	0	/* Running on SUN workstation system */
#define SYSV	0	/* Running on Unix System V (or perhaps Sys III) */
#define TOPS20	0	/* Running on TOPS-20 KCC C implementation */
#define UCB	0	/* Running on 2.8, 2.9, or 4.x BSD sys (tty stuff) */
#define VENIX86 0	/* Running on Venix86 (IBM-PC) system */

#define ICONOGRAPHICS 0 /* Using Iconographics configuration version */
#define IMAGEN 0	/* Using Imagen configuration version */

/* Resolve system dependencies */
#if SUN
#undef BSD4_2
#define BSD4_2 1	/* SUN uses 4.2BSD */
#endif

#if BSD4_2
#undef UCB
#define UCB	1	/* 4.2 is special case of general UCB stuff */
#endif /*BSD4_2*/

#if (PCIX || HPUX)
#undef SYSV
#define SYSV	1	/* PC/IX & HP-UX are based on System III & V (resp) */
#endif

/* Set system or site dependent stuff here */

#if V6
#define void int	/* May need this for other systems too */
#endif

/* Changes to parameters (elle.h) or variable defaults (e_vinit.c) */

#if COHERENT
#define EVFNO2  0	/* "Old" filename postfix - use no old file! */
#define EVFNN2 "+"	/* "New" filename postfix */
#define TX_COHIBM 1	/* Ensure Coherent IBM-PC console support included */
#endif /*COHERENT*/

#if DNTTY
#define EVLLEN 60	/* Short line length for TDDs */
#endif /*DNTTY*/

#if HPUX
#define EVFNO2 "~"	/* Same as CCA Emacs.  Sorts last in listing. */
#endif /*HPUX*/

#if MINIX
#define EVFNO2 ".bak"	/* "Old" filename postfix */
#define EVMARKSHOW "Mark set"
#define EVCCOL (33)	/* Use this as Comment Column */
#define EVMVPCT 1	/* 1% - Try to use minimal window repositioning */
#define EVMODWSO 1	/* Use mode window standout if can */
#define STRERROR 1	/* Say that implementation provides strerror() */

#include <sys/types.h>
#include <string.h>
#include <unistd.h>
#include <stdio.h>
#endif /*MINIX*/

#if ONYX
#define STKMEM (4*512)		/* ONYX Z8000 seems to have less room avail */
#endif /*ONYX*/

#if BSD4_2
#define FNAMELEN 255	/* Max size of last filename component */
#define FNAMSIZ 400	/* Max size of complete filename */
#endif /*BSD4_2*/

#if TOPS20
#define EVHELPFILE "elle:help.dat"	/* T20 ELLE help file */
#define EVPROFBINFILE "ellep.b1"	/* T20 binary profile file */
#define EVPROFTEXTFILE "ellep.e"	/* T20 ASCII profile file */
#define EVFNO2 0	/* No old filename postfix (T20 has generations) */
#define EVFNN2 0	/* No new filename postfix (T20 has generations) */
#define FNAMELEN (40*3)	/* Max size of non-directory filename component */
#define FNAMSIZ (40*5)	/* Max size of complete filename */
#define STRERROR 1	/* Say that implementation provides strerror() */
#endif /*TOPS20*/

#if VENIX86
#define TIBFSIZ 1	/* Venix86 block reads in raw mode */
#endif /*VENIX86*/

/* Configuration settings */

#if ICONOGRAPHICS
#define EVFNO2 "@"	/* "Old" filename postfix */
#define EVMARKSHOW "Set."
#define PARABLOCK 1	/* Values meaningful only for ICONOGRAPHICS */
#define PARALINE  2
#define TXC_VISBEL 1	/* Use visible bell if possible */
#endif /*ICONOGRAPHICS*/

#if IMAGEN
#define EVFNO2 ".BAK"	/* "Old" filename postfix */
#define EVMARKSHOW "Mark set"
#define TOBFSIZ (10*80)	/* Size of TTY output buffer */
#define ECHOLINES 2	/* Use 2 echo-area lines, not 1 */
#define MAXARGFILES 10	/* Several startup filename args */
#endif /*IMAGEN*/

/* Now set any defaults for things not already defined */

/* TERMINAL SUPPORT SWITCHES */
/* 	Only those terminals which have a switch defined here	*/
/*	will be included in ELLE's "hardwired" support.		*/
/*	Switch name:	Compiles support for:			*/
#ifndef TX_TERMCAP
#define TX_TERMCAP 1	/*    *	- most TERMCAP-defined terminals */
#endif
#ifndef TX_H19
#define TX_H19	1	/* "H19"	- Heath/Zenith-19 */
#endif
#ifndef TX_DM2500
#define TX_DM2500 1	/* "DM2500","DM3025" - Datamedia 2500 */
#endif
#ifndef TX_COHIBM
#define TX_COHIBM 0	/* "COHIBM"	- Coherent IBM-PC console */
#endif
#ifndef TX_TVI925
#define TX_TVI925 0	/* "TVI925"	- TeleVideo 925 */
#endif
#ifndef TX_OM8025
#define TX_OM8025 0	/* "OM8025"	- Omron 8025AG */
#endif

#ifndef TXC_VISBEL	/* Non-zero if want to use visible bell */
#define TXC_VISBEL 0
#endif

/* Default terminal type string, used if ELLE cannot get type either
** from $TERM or from startup args.
*/
#ifndef TXS_DEFAULT
#define TXS_DEFAULT "H19"	/* Default terminal type string */
#endif

/* Combination parameter/switch definitions */

/* STKMEM - System-dependent stack allocation crock, defines amount of
 *	stack memory to grab for general-purpose use.  This is mainly
 *	useful for PDP-11s or machines with similarly brain-damaged
 *	address space hardware.  A PDP-11 memory segment is 8K bytes,
 *	or 16 512-byte blocks, and the stack segment quarantines all of
 *	this space even though the actual stack may only use a miniscule
 *	portion of it.
 */

/* Use this if compiling for a PDP11 system, otherwise leave undefined.. */
#if (V6 || 0)
#define STKMEM (8*512)		/* Use half a PDP11 segment */
#endif

/* These defaults are in eesite.h so ELLEC can get at them too. */
#ifndef EVPROFBINFILE	/* Location of binary user profile, relative to HOME */
#define EVPROFBINFILE ".ellepro.b1"
#endif
#ifndef EVPROFTEXTFILE	/* Location of ASCII user profile (used by ELLEC) */
#define EVPROFTEXTFILE ".ellepro.e"
#endif
