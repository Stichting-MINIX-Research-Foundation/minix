/*	$NetBSD: screen.h,v 1.2 2011/11/23 19:25:28 tnozaki Exp $ */

/*-
 * Copyright (c) 1992, 1993, 1994
 *	The Regents of the University of California.  All rights reserved.
 * Copyright (c) 1992, 1993, 1994, 1995, 1996
 *	Keith Bostic.  All rights reserved.
 *
 * See the LICENSE file for redistribution information.
 *
 *	Id: screen.h,v 10.49 2002/03/02 23:47:02 skimo Exp (Berkeley) Date: 2002/03/02 23:47:02
 */

/*
 * There are minimum values that vi has to have to display a screen.  The row
 * minimum is fixed at 1 (the svi code can share a line between the text line
 * and the colon command/message line).  Column calculation is a lot trickier.
 * For example, you have to have enough columns to display the line number,
 * not to mention guaranteeing that tabstop and shiftwidth values are smaller
 * than the current column value.  It's simpler to have a fixed value and not
 * worry about it.
 *
 * XXX
 * MINIMUM_SCREEN_COLS is almost certainly wrong.
 */
#define	MINIMUM_SCREEN_ROWS	 1
#define	MINIMUM_SCREEN_COLS	20

/*
 * WIN --
 *	A list of screens that are displayed as a whole.
 */
struct _win {
	CIRCLEQ_ENTRY(_win)	    q;	    /* Windows. */

	CIRCLEQ_HEAD(_scrh, _scr)   scrq;   /* Screens */

	GS	*gp;			/* Pointer to global area. */

	SCR	*ccl_sp;		/* Colon command-line screen. */

	void	*perl_private;		/* Perl interpreter. */

	void	*ip_private;		/* IP support private area. */

	void	*th_private;		/* Threading support private area. */

	/*
	 * Ex command structures (EXCMD).  Defined here because ex commands
	 * exist outside of any particular screen or file.
	 */
#define	EXCMD_RUNNING(wp)	((wp)->ecq.lh_first->clen != 0)
	LIST_HEAD(_excmdh, _excmd) ecq;	/* Ex command linked list. */
	EXCMD	 	excmd;		/* Default ex command structure. */
	char	       *if_name;	/* Current associated file. */
	db_recno_t	if_lno;		/* Current associated line number. */

	EVENT	*i_event;		/* Array of input events. */
	size_t	 i_nelem;		/* Number of array elements. */
	size_t	 i_cnt;			/* Count of events. */
	size_t	 i_next;		/* Offset of next event. */

	CB	*dcbp;			/* Default cut buffer pointer. */
	CB	 dcb_store;		/* Default cut buffer storage. */
	LIST_HEAD(_cuth, _cb) cutq;	/* Linked list of cut buffers. */

	/* For now, can be either char or CHAR_T buffer */
	char	*tmp_bp;		/* Temporary buffer. */
	size_t	 tmp_blen;		/* Temporary buffer size. */

	char	*l_lp;			/* Log buffer. */
	size_t	 l_len;			/* Log buffer length. */

	CONVWIN	 cw;

/* Flags. */
#define	W_TMP_INUSE	0x0001		/* Temporary buffer in use. */
	u_int32_t flags;

					/* Message or ex output. */
	void	(*scr_msg) __P((SCR *, mtype_t, char *, size_t));
};

/*
 * SCR --
 *	The screen structure.  To the extent possible, all screen information
 *	is stored in the various private areas.  The only information here
 *	is used by global routines or is shared by too many screens.
 */
struct _scr {
/* INITIALIZED AT SCREEN CREATE. */
	CIRCLEQ_ENTRY(_scr) q;		/* Screens. */
	CIRCLEQ_ENTRY(_scr) eq;         /* Screens. */

	int	 id;			/* Screen id #. */
	int	 refcnt;		/* Reference count. */

	WIN	*wp;			/* Pointer to window. */
	GS	*gp;			/* Pointer to global area. */
	SCR	*nextdisp;		/* Next display screen. */
	SCR	*ccl_parent;		/* Colon command-line parent screen. */
	EXF	*ep;			/* Screen's current EXF structure. */

	CHAR_T	*c_lp;			/* Cached line. */
	size_t	 c_len;			/* Cached line length. */
	/* May move out again once we use DB 
	 * to cache internal representation
	 */
	size_t	 c_blen;		/* Cached line buffer length. */
	db_recno_t	 c_lno;		/* Cached line number. */

	FREF	*frp;			/* FREF being edited. */
	char	**argv;			/* NULL terminated file name array. */
	char	**cargv;		/* Current file name. */

	u_long	 ccnt;			/* Command count. */
	u_long	 q_ccnt;		/* Quit or ZZ command count. */

					/* Screen's: */
	size_t	 rows;			/* 1-N: number of rows. */
	size_t	 cols;			/* 1-N: number of columns. */
	size_t	 t_rows;		/* 1-N: cur number of text rows. */
	size_t	 t_maxrows;		/* 1-N: max number of text rows. */
	size_t	 t_minrows;		/* 1-N: min number of text rows. */
	size_t	 coff;			/* 0-N: screen col offset in display. */
	size_t	 roff;			/* 0-N: screen row offset in display. */

					/* Cursor's: */
	db_recno_t	 lno;			/* 1-N: file line. */
	size_t	 cno;			/* 0-N: file character in line. */

	size_t	 rcm;			/* Vi: 0-N: Most attractive column. */

#define	L_ADDED		0		/* Added lines. */
#define	L_CHANGED	1		/* Changed lines. */
#define	L_DELETED	2		/* Deleted lines. */
#define	L_JOINED	3		/* Joined lines. */
#define	L_MOVED		4		/* Moved lines. */
#define	L_SHIFT		5		/* Shift lines. */
#define	L_YANKED	6		/* Yanked lines. */
	db_recno_t	 rptlchange;		/* Ex/vi: last L_CHANGED lno. */
	db_recno_t	 rptlines[L_YANKED + 1];/* Ex/vi: lines changed by last op. */

	TEXTH	 tiq;			/* Ex/vi: text input queue. */

	SCRIPT	*script;		/* Vi: script mode information .*/

	db_recno_t	 defscroll;	/* Vi: ^D, ^U scroll information. */

					/* Display character. */
	u_char	 cname[MAX_CHARACTER_COLUMNS + 1];
	size_t	 clen;			/* Length of display character. */

	enum {				/* Vi editor mode. */
	    SM_APPEND = 0, SM_CHANGE, SM_COMMAND, SM_INSERT,
	    SM_REPLACE } showmode;

	void	*ex_private;		/* Ex private area. */
	void	*vi_private;		/* Vi private area. */
	void	*perl_private;		/* Perl private area. */
	void	*cl_private;		/* Curses private area. */

	CONV	conv;

	struct _log_state  state;	/* State during log traversal. */

/* PARTIALLY OR COMPLETELY COPIED FROM PREVIOUS SCREEN. */
	char	*alt_name;		/* Ex/vi: alternate file name. */

	ARG_CHAR_T	 at_lbuf;	/* Ex/vi: Last executed at buffer. */

					/* Ex/vi: re_compile flags. */
#define	RE_WSTART	L("[[:<:]]")	/* Ex/vi: not-in-word search pattern. */
#define	RE_WSTOP	L("[[:>:]]")
#define RE_WSTART_LEN	(sizeof(RE_WSTART)/sizeof(CHAR_T)-1)
#define RE_WSTOP_LEN	(sizeof(RE_WSTOP)/sizeof(CHAR_T)-1)
					/* Ex/vi: flags to search routines. */
#define	SEARCH_CSCOPE	0x000001	/* Search for a cscope pattern. */
#define	SEARCH_CSEARCH	0x000002	/* Compile search replacement. */
#define	SEARCH_CSUBST	0x000004	/* Compile substitute replacement. */
#define	SEARCH_EOL	0x000008	/* Offset past EOL is okay. */
#define	SEARCH_EXTEND	0x000010	/* Extended RE. */
#define	SEARCH_FIRST	0x000020	/* Search from the first line. */
#define	SEARCH_IC	0x000040	/* Ignore case. */
#define	SEARCH_ICL	0x000080	/* Ignore case. */
#define	SEARCH_INCR	0x000100	/* Search incrementally. */
#define	SEARCH_LITERAL	0x000200	/* Literal string. */
#define	SEARCH_MSG	0x000400	/* Display search messages. */
#define	SEARCH_NOOPT	0x000800	/* Ignore edit options. */
#define	SEARCH_PARSE	0x001000	/* Parse the search pattern. */
#define	SEARCH_SET	0x002000	/* Set search direction. */
#define	SEARCH_TAG	0x004000	/* Search for a tag pattern. */
#define	SEARCH_WMSG	0x008000	/* Display search-wrapped messages. */
#define	SEARCH_WRAP	0x010000	/* Wrap past sof/eof. */

					/* Ex/vi: RE information. */
	dir_t	 searchdir;		/* Last file search direction. */
	regex_t	 re_c;			/* Search RE: compiled form. */
	CHAR_T	*re;			/* Search RE: uncompiled form. */
	size_t	 re_len;		/* Search RE: uncompiled length. */
	regex_t	 subre_c;		/* Substitute RE: compiled form. */
	CHAR_T	*subre;			/* Substitute RE: uncompiled form. */
	size_t	 subre_len;		/* Substitute RE: uncompiled length). */
	CHAR_T	*repl;			/* Substitute replacement. */
	size_t	 repl_len;		/* Substitute replacement length.*/
	size_t	*newl;			/* Newline offset array. */
	size_t	 newl_len;		/* Newline array size. */
	size_t	 newl_cnt;		/* Newlines in replacement. */
	u_int8_t c_suffix;		/* Edcompatible 'c' suffix value. */
	u_int8_t g_suffix;		/* Edcompatible 'g' suffix value. */

	OPTION	 opts[O_OPTIONCOUNT];	/* Ex/vi: Options. */

/*
 * Screen flags.
 *
 * Editor screens.
 */
#define	SC_EX		0x00000001	/* Ex editor. */
#define	SC_VI		0x00000002	/* Vi editor. */

/*
 * Screen formatting flags, first major, then minor.
 *
 * SC_SCR_EX
 *	Ex screen, i.e. cooked mode.
 * SC_SCR_VI
 *	Vi screen, i.e. raw mode.
 * SC_SCR_EXWROTE
 *	The editor had to write on the screen behind curses' back, and we can't
 *	let curses change anything until the user agrees, e.g. entering the
 *	commands :!utility followed by :set.  We have to switch back into the
 *	vi "editor" to read the user's command input, but we can't touch the
 *	rest of the screen because it's known to be wrong.
 * SC_SCR_REFORMAT
 *	The expected presentation of the lines on the screen have changed,
 *	requiring that the intended screen lines be recalculated.  Implies
 *	SC_SCR_REDRAW.
 * SC_SCR_REDRAW
 *	The screen doesn't correctly represent the file; repaint it.  Note,
 *	setting SC_SCR_REDRAW in the current window causes *all* windows to
 *	be repainted.
 * SC_SCR_CENTER
 *	If the current line isn't already on the screen, center it.
 * SC_SCR_TOP
 *	If the current line isn't already on the screen, put it at the to@.
 */
#define	SC_SCR_EX	0x00000004	/* Screen is in ex mode. */
#define	SC_SCR_VI	0x00000008	/* Screen is in vi mode. */
#define	SC_SCR_EXWROTE	0x00000010	/* Ex overwrite: see comment above. */
#define	SC_SCR_REFORMAT	0x00000020	/* Reformat (refresh). */
#define	SC_SCR_REDRAW	0x00000040	/* Refresh. */

#define	SC_SCR_CENTER	0x00000080	/* Center the line if not visible. */
#define	SC_SCR_TOP	0x00000100	/* Top the line if not visible. */

/* Screen/file changes. */
#define	SC_EXIT		0x00000200	/* Exiting (not forced). */
#define	SC_EXIT_FORCE	0x00000400	/* Exiting (forced). */
#define	SC_FSWITCH	0x00000800	/* Switch underlying files. */
#define	SC_SSWITCH	0x00001000	/* Switch screens. */

#define	SC_ARGNOFREE	0x00002000	/* Argument list wasn't allocated. */
#define	SC_ARGRECOVER	0x00004000	/* Argument list is recovery files. */
#define	SC_AT_SET	0x00008000	/* Last at buffer set. */
#define	SC_COMEDIT	0x00010000	/* Colon command-line edit window. */
#define	SC_EX_GLOBAL	0x00020000	/* Ex: executing a global command. */
#define	SC_EX_SILENT	0x00040000	/* Ex: batch script. */
#define	SC_EX_WAIT_NO	0x00080000	/* Ex: don't wait for the user. */
#define	SC_EX_WAIT_YES	0x00100000	/* Ex:    do wait for the user. */
#define	SC_READONLY	0x00200000	/* Persistent readonly state. */
#define	SC_RE_SEARCH	0x00400000	/* Search RE has been compiled. */
#define	SC_RE_SUBST	0x00800000	/* Substitute RE has been compiled. */
#define	SC_SCRIPT	0x01000000	/* Shell script window. */
#define	SC_STATUS	0x02000000	/* Welcome message. */
#define	SC_STATUS_CNT	0x04000000	/* Welcome message plus file count. */
#define	SC_TINPUT	0x08000000	/* Doing text input. */
#define	SC_TINPUT_INFO	0x10000000	/* Doing text input on info line. */
#define SC_CONV_ERROR	0x20000000	/* Met with a conversion error. */
	u_int32_t flags;

	int	    db_error;		/* Return code from db function. */
};
