/* ELLE - Copyright 1982, 1987 by Ken Harrenstien, SRI International
 *	This software is quasi-public; it may be used freely with
 *	like software, but may NOT be sold or made part of licensed
 *	products without permission of the author.
 */
/* EEVINI - ELLE initialized variables and array storage.
 *	Initial values are defined here, but the vars must be
 * declared in ELLE.H as well so that references from all modules will
 * compile correctly.
 *	Arrays are also allocated here, so size re-definitions only require
 * re-compiling this single small module.
 */

#define EXT		/* Allocate storage for non-initialized vars */
#include "elle.h"

#ifndef EVFILMOD
#if V6
#define EVFILMOD (0600)	/* (int) Default file creation mode on V6 */
#else
#define EVFILMOD (0666)	/* (int) Default file creation mode on V7, note */
#endif /*-V6*/		/*       paranoids can use V7 "umask" in shell. */
#endif
#ifndef EVFNO1
#define EVFNO1 0	/* (char *) "Old" filename prefix */
#endif
#ifndef EVFNN1
#define EVFNN1 0	/* (char *) "New" filename prefix */
#endif
#ifndef EVFNO2
#define EVFNO2 "O"	/* (char *) "Old" filename postfix */
#endif
#ifndef EVFNN2
#define EVFNN2 "N"	/* (char *) "New" filename postfix */
#endif
#ifndef EVFCOL
#define EVFCOL (71)	/* (int) Initial value for Fill Column */
#endif
#ifndef EVCCOL
#define EVCCOL (40)	/* (int) Initial value for Comment Column */
#endif
#ifndef EVNWPCT
#define EVNWPCT 50	/* (int) 50% For random New Window, center cursor. */
#endif
#ifndef EVMVPCT
#define EVMVPCT 67	/* (int) 67% When move off edge, show 67% new stuff */
#endif
#ifndef EVMODWSO
#define EVMODWSO 0	/* (bool) Initial mode window standout mode */
#endif
#ifndef EV2MODEWS
#define EV2MODEWS 1	/* 2-mode-window flag. 0=Never, 1=if SO, 2=always */
#endif
#ifndef EVMARKSHOW
#define EVMARKSHOW 0	/* (char *) String shown for Set Mark */
#endif
#ifndef EVHELPFILE	/* (char *) Location of ELLE help file. */
#define EVHELPFILE "/usr/src/elle/help.dat"
#endif

char *ev_verstr = "ELLE 4.1b";	/* String: Editor name and version # */
int ev_filmod = EVFILMOD;	/* Default file creation mode */
char *ev_fno1 = EVFNO1;		/* "Old" filename prefix */
char *ev_fnn1 = EVFNN1;		/* "New" filename prefix */
char *ev_fno2 = EVFNO2;		/* "Old" filename postfix */
char *ev_fnn2 = EVFNN2;		/* "New" filename postfix */

int ev_fcolumn = EVFCOL;	/* Initial value for Fill Column */
#if FX_INDCOMM
int ev_ccolumn = EVCCOL;	/* Initial value for Comment Column */
#endif

/* New window selection parameters.
**	Both are expressed as an integer % of window lines (where the whole
**	window is 100), and apply when a new window is selected.
** ev_nwpct - when "New Window" is called, % of lines between top and cursor.
**	Also applies when screen has changed randomly.
** ev_mvpct - when cursor moves out of window, this is the % of lines
**	between top and cursor (if moved off top) or between bottom and
**	cursor (if moved off bottom).
*/
int ev_nwpct = EVNWPCT;		/* New Window cursor loc preference (%) */
int ev_mvpct = EVMVPCT;		/* Moved cursor loc preference (%) */

#if FX_SOWIND
int ev_modwso = EVMODWSO;	/* Initial mode window standout flag */
#endif
#if FX_2MODEWINDS
int ev_2modws = EV2MODEWS;	/* Initial 2-mode-wind flag */
#endif
char *ev_markshow = EVMARKSHOW;	/* String to display when Set Mark done */

char *ev_helpfile = EVHELPFILE;	/* Location of ELLE help file */
char *ev_profile = EVPROFBINFILE; /* Location of ELLE binary user profile */
				/* Note ELLE doesn't use EVPROFTEXTFILE. */

/* Array allocations */

struct scr_line *scr[MAXHT];		/* Array of screen line structures */
SBSTR *kill_ring[KILL_LEN];		/* Kill ring table */
