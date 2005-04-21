/* ELLE - Copyright 1982, 1984 by Ken Harrenstien, SRI International
 *	This software is quasi-public; it may be used freely with
 *	like software, but may NOT be sold or made part of licensed
 *	products without permission of the author.
 */
/*
 * ELLE.H	Global ELLE definitions
 */

#ifndef EXT
#define EXT extern	/* Default assumes these are referencing decls */
#endif

/* Make identifiers unique in 1st 6 chars as per ANSI rule for externals */
#define tvc_cin tvccin
#define tvc_cdn tvccdn
#define tvc_lin tvclin
#define tvc_ldn tvcldn
#define ev_fno1 evfno1
#define ev_fno2 evfno2
#define ev_fnn1 evfnn1
#define ev_fnn2 evfnn2

#define ask_sall	asksal	/* eebuff.c */
#define ask_save	asksav
#define buf_tmod	buftmo
#define buf_tmat	buftma
#define e_gobob		egobob	/* eeedit.c */
#define e_gobol		egobol
#define e_goeob		egoeob
#define e_goeol		egoeol
#define fill_prefix	filpfx	/* eefill.c */
#define fill_plen	filpln
#define fill_cur_line	filcln
#define kill_ptr	kilptr		/* eef3.c */
#define kill_push	kilpsh
#define ed_insert	edinst	/* eefed.c */
#define ed_insn		edinsn
#define ed_deln		eddeln
#define ed_delete	eddele
#define f_fillreg	ffilrg	/* eejust.c */
#define f_fillpara	ffilpa

#include "eesite.h"	/* Insert site-dependent flags and parameters */
#include "sb.h"		/* Insert SB package definitions */
#include "eeprof.h"	/* Insert user profile definition.  This is a
			 *	separate file so ELLEC can use it too. */
#include "eefidx.h"	/* Insert desired function defs */

/* ELLE Compile-time parameter defaults */

#ifndef KILL_LEN
#define KILL_LEN 8	/* Size of kill ring */
#endif
#ifndef MAXHT
#define MAXHT 72	/* Height (# lines) of largest screen we'll suport */
#endif
#ifndef MAXLINE
#define MAXLINE 132	/* Width  (# chars) of largest screen we'll support */
#endif
#ifndef FNAMELEN
#define FNAMELEN 14	/* Sys-dep: Max size of last filename component */
#endif			/*	Check FNAMSIZ if you change this. */
#ifndef FNAMSIZ
#define FNAMSIZ 100	/* Sys-dep: Max size of complete filename */
#endif			/*	This must be at least as large as FNAMELEN! */
#ifndef ISRCHLIM
#define ISRCHLIM 50	/* Max # of chars to allow I-search on */
#endif
#ifndef TOBFSIZ
#define TOBFSIZ 80	/* Size of TTY output buffer */
#endif
#ifndef TIBFSIZ
#define TIBFSIZ 50	/* Size of TTY input buffer */
#endif
#ifndef ECHOLINES
#define ECHOLINES 1	/* # of lines for echo area (below mode line) */
#endif
#ifndef MAXARGFILES
#define MAXARGFILES 2	/* # of filename args OK at startup */
#endif

/* ELLE initialized variables.
 *	Initial values are defined in EEVINI.C, but the vars must be
 * declared here as well so that references from all modules will
 * compile correctly.
 */

extern char *ev_verstr;		/* String: Editor name and version # */
extern int ev_filmod;		/* Default file creation mode */
extern char *ev_fno1,*ev_fno2;	/* Pre, postfix for "old" filenames */
extern char *ev_fnn1,*ev_fnn2;	/* Pre, postfix for "new" filenames */
extern int ev_fcolumn;		/* Fill Column variable */
#if FX_INDCOMM
extern int ev_ccolumn;		/* Comment Column variable */
#endif
extern int ev_nwpct, ev_mvpct;	/* New window selection percentages */
#if FX_SOWIND
extern int ev_modwso;		/* Initial mode window standout flag */
#endif
#if FX_2MODEWINDS
extern int ev_2modws;		/* Initial setting of 2-mode-window flag */
#endif
extern char *ev_markshow;	/* String to show when Set Mark done */
extern char *ev_helpfile;	/* Location of ELLE help file */
extern char *ev_profile;	/* Filename of ELLE binary user profile */
extern struct profile def_prof;	/* ELLE default user profile */

/* Global variables */

EXT chroff cur_dot;		/* Current dot */
EXT chroff mark_dot;		/* Dot for mark */
EXT int mark_p;			/* flag indicating whether mark exists */
EXT int this_cmd, last_cmd;	/* Command type */
EXT int unrchf;			/* Stuffed character back for readcommand */
EXT int exp;			/* Numeric argument for commands */
EXT int exp_p;			/* Flag meaning an arg was given */
EXT int pgoal;			/* Permanent goal column */
EXT int goal;
EXT char *srch_str;		/* Last search string specified (0 = none) */
EXT int srch_len;		/* Length of srch_str string */
EXT int ask_len;		/* Length of last string returned by "ask" */
EXT char *homedir;		/* User's home directory */
EXT int kill_ptr;		/* Index into kill ring */
extern SBSTR *kill_ring[];	/* Kill ring table (allocated in eevini) */

/* Editor Command types */

#define KILLCMD 1		/* Kill command, for kill merging */
#define ARGCMD  2		/* Argument-setter, for main loop */
#define YANKCMD 3		/* Yank command, for yankpop */
#define LINECMD 4		/* Next or previous line goal hacking */
#if IMAGEN
#define INSCMD  5		/* Simple char-insert command, for autowrap */
#endif /*IMAGEN*/

/* Misc char definitions */
#define CTRL(ch) (037&ch)
#define BELL	('\007')	/* Will become \a in ANSI */
#define BS	('\b')
#define TAB	('\t')
#define LF	('\n')
#define FF	('\f')
#define CR	('\r')
#define ESC	('\033')
#define SP	(' ')
#define DEL	('\177')

#define CB_META (0200)		/* Meta bit in command char */
#define CB_EXT  (0400)		/* Extend bit in command char */
#define METIZER ESC
#define EXTIZER CTRL('X')

/* Terminal parameters - set at runtime startup */

EXT char *tv_stype;	/* Terminal type string specified by user/system */
EXT int scr_ht;		/* # lines of main screen area */
EXT int scr_wid;	/* # columns of screen */
EXT int scr_wd0;	/* scr_wid - 1 (for 0-origin stuff) */
EXT int trm_ospeed;	/* Output speed index */
EXT int tvc_pos;	/* Cost for absolute move (# of output chars) */
EXT int tvc_bs;		/* Cost for backspace */
EXT int tvc_ci, tvc_cin;	/* Char ins cost per call, cost per column */
EXT int tvc_cd, tvc_cdn;	/* Char del   "   "   "     "    "   "     */
EXT int tvc_li, tvc_lin;	/* Line ins cost per call, cost per line */
EXT int tvc_ld, tvc_ldn;	/* Line del   "   "   "     "    "   "   */

EXT int trm_flags;	/* Terminal capabilities - bit flags */
			/* Maybe change to word vars someday (faster) */
#define TF_IDLIN	01	/* Has I/D line */
#define TF_IDCHR	02	/* Has I/D char */
#define TF_SO		04	/* Has usable standout mode */
#define TF_CLEOL	010	/* Has clear-to-eol */
#define TF_METAKEY	020	/* Has meta key */
#define TF_DIRVID	040	/* Has direct-video type interface */


/* Redisplay definitions */

EXT int curs_lin;	/* Line # of current cursor (0 origin) */
EXT int curs_col;	/* Column # of current cursor (0 origin) */

EXT int rd_type;	/* Global var: holds redisplay "hints" */
#define redp(n) rd_type |= (n)

#define RD_SCREEN 01	/* Clear everything and redisplay */
#define RD_WINDS 02	/* Check all windows for changes (b/emod) */
#define RD_MODE 04	/* Mode line has changed, update it. */
#define RD_WINRES 0400	/* Assume all of window was changed (clear b/emod) */
#define RD_MOVE	010	/* Cursor has moved */
#define RD_UPDWIN 020	/* Window fixed, must update modified screen lines */
/*#define RD_ICHR 0	*//* Hint: Char insert done */
/*#define RD_DCHR 0	*//* Hint: Char del done */
#define RD_ILIN 0100	/* Hint: Line insert done */
#define RD_DLIN 0200	/* Hint: Line del done */

/* #define RD_MOVWIN 02000	*//* Window should be re-positioned */
#define RD_FIXWIN 02000		/* Window needs fixing (call fix_wind) */
#define RD_TMOD   04000		/* Text changed in this window, check it. */
#define RD_WINCLR 010000	/* Clear window with CLEOLs (not yet) */
#define RD_CHKALL 020000	/* Check all windows for redisplay flags */
#if IMAGEN
#define RD_REDO   040000	/* Just re-do the entire window, don't think */
#endif /*IMAGEN*/

	/* Flags with global effects, only seen in rd_type */
#define RDS_GLOBALS (RD_SCREEN|RD_MODE|RD_WINDS|RD_CHKALL)
	/* Flags which are allowed per-window (in w_redp) */
#define RDS_WINFLGS (~RDS_GLOBALS)
	/* Flags which force FIX_WIND() to do something */
#define RDS_DOFIX (RD_WINRES|RD_TMOD|RD_FIXWIN|RD_MOVE)

#define CI_CLINE '!'		/* Char indicator for continued line */
#define CI_CNTRL '^'		/* Char indicator for control chars */
#define CI_META  '~'		/* Char indicator for meta-bit (8th) set */
#define CI_TOP   '|'		/* Char indicator for top-bit (9th) set */
#define MAXCHAR (8+3)		/* Longest char representation (TAB) + slop */

/* Definitions for screen structures */

struct scr_line {
	chroff sl_boff;		/* Ptr to start of line's text in buffer */
	int sl_len;		/* # buffer chars in line (incl NL) */
	char *sl_line;		/* Ptr to screen image of line */
	int sl_col;		/* # chars in image == # columns used */
	char sl_flg;		/* Flags - set if this line modified */
	char sl_cont;		/* If line being continued on next, this */
				/* contains 1 plus # extra chars (if any) */
				/* stored at end of this line which shd be */
				/* put at beg of next line. */
	char *sl_nlin;	/* New screen image line if modified flag set */
	int sl_ncol;
};
	/* sl_flg definitions */
#define SL_MOD 01		/* New line exists, must update to it */
#define SL_EOL 02		/* Buffer line ends with EOL */
#define SL_CSO 04		/* Current screen line is in standout mode */
#define SL_NSO 010		/* New screen line is in standout mode */
#if IMAGEN
#define SL_REDO 0100		/* Line should be redone completely */
#endif /*IMAGEN*/

extern struct scr_line *scr[];	/* Screen line ptrs (allocated in e_vinit) */


/* Buffer stuff */

struct buffer 
{	SBBUF b_sb;			/* MUST be 1st thing! */
	struct buffer *b_next;		/* ptr to next in chain */
	char *b_name;			/* text name */
	char *b_fn;			/* filename */
	chroff b_dot;			/* point (dot) */
	int b_flags;			/* misc. bits */
	struct majmode *b_mode;		/* Mode of buffer */
#if IMAGEN
	long b_mtime;			/* Last file modification time */
#endif /*IMAGEN*/
};
	/* b_flags definitions */
#define B_MODIFIED	01		/* Buffer is modified */
#define B_EOLCRLF	0200		/* On = CRLF mode, off = LF mode */
#if IMAGEN
#define B_PERMANENT 002			/* buffer cannot be killed */
#define B_CMODE	    004			/* "C" mode (HACK HACK) */
#define B_BACKEDUP  010			/* Buffer has been backed up once */
#define B_TEXTMODE  020			/* Text mode (auto-wrap, basically) */
#define B_QUERYREP  040			/* Query-replace mode (qualifier) */
#endif /*IMAGEN*/

/* Handy macro to check EOL mode */
#define eolcrlf(buf) (((struct buffer *)buf)->b_flags&B_EOLCRLF)

/* Buffer pointers */

EXT struct buffer
		 *buf_head,		/* head of list of all buffers */
		 *cur_buf,		/* buffer we are editing now */
		 *last_buf,		/* buffer we were editing before */
		 *lines_buf;		/* buffer for sep_win */

/* Window stuff */

struct window
{	struct window *w_next;		/* ptr to next in chain */
	int w_flags;			/* Window flags */
	int w_pos;			/* index of top line */
	int w_ht;			/* number of lines */
	struct buffer *w_buf;		/* buffer in this window */
	int w_pct;			/* % of buffer window is at */
	int w_redp;			/* Redisplay hints */
	chroff w_topldot;		/* line currently at top of window */
	chroff w_dot;			/* Saved dot while not cur_win */
	chroff w_bmod;			/* Lower bound of modified text */
	chroff w_emod;			/* Upper bound of modified text */
					/* (offset from end of buffer!) */
	chroff w_oldz;			/* Buffer len as of last update */
};

/* Window flags */
#define W_STANDOUT	01	/* Use terminal's standout mode for window */
#define W_MODE		02	/* This is a mode window */

/* Window pointers */

EXT struct window
		*win_head,		/* head of list of all windows */
		*cur_win,		/* window we are now in */
		*user_win,		/* current user window */
		*oth_win,		/* "other" user window */
		*mode_win,		/* window for mode line */
		*ask_win,		/* window for ask (echo) area */
		*sep_win;		/* window for separation dashes */

/* Major Mode stuff.  Each buffer has its own major mode.
 * Only one major mode may be in effect at any time.
 */
struct majmode {
	char *mjm_name;		/* Simple for now */
};
EXT struct majmode *fun_mode;	/* Fundamental mode - the default */
EXT struct majmode *cur_mode;	/* Current major mode */

/* Minor modes are currently implemented by means of flag variables
 * which have global effects (regardless of buffer or major mode).
 * Each variable has the name "x_mode" where x is the name of the minor
 * mode.  These are declared in the modules containing their support code.
 * In the future this may be generalized along the lines of major modes.
 */


/* Miscellaneous debug stuff */

EXT int dbgval;		/* Set nonzero to do verify stuff */
EXT int dbg_isw;	/* Set to enable interrupts if possible */
#if IMAGEN
EXT int dbg_redp;	/* Set to debug redisplay algorithms */
#endif /*IMAGEN*/
extern int errno;

/* V7 routines for setexit/reset emulation */

#if !(V6)
#include <setjmp.h>
EXT jmp_buf env_main;
#define setexit(a)	setjmp(env_main)
#define reset(a)	longjmp(env_main,a)
#endif /*-V6*/

/* Declare functions returning CHROFF values (offsets into a buffer) */

extern chroff e_dot(),e_nldot(),e_pldot(),e_boldot(),e_eoldot(),
	e_alldot(),ex_boldot(),ex_alldot(),
	ex_blen(),e_blen(),ex_dot(),e_wdot();

extern SBSTR *e_copyn();

/* Some other commonly needed declarations */

extern char *memalloc(), *ask(), *dottoa(), *strdup();
#if !(V6)
extern char *getenv();
#endif /*-V6*/
#include "eeproto.h"	/* function prototypes */
