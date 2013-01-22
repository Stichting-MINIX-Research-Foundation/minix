/*	$NetBSD: gs.h,v 1.5 2011/03/21 14:53:02 tnozaki Exp $ */

/*-
 * Copyright (c) 1993, 1994
 *	The Regents of the University of California.  All rights reserved.
 * Copyright (c) 1993, 1994, 1995, 1996
 *	Keith Bostic.  All rights reserved.
 *
 * See the LICENSE file for redistribution information.
 *
 *	Id: gs.h,v 10.55 2001/11/01 10:28:25 skimo Exp (Berkeley) Date: 2001/11/01 10:28:25
 */

#define	TEMPORARY_FILE_STRING	"/tmp"	/* Default temporary file name. */

/*
 * File reference structure (FREF).  The structure contains the name of the
 * file, along with the information that follows the name.
 *
 * !!!
 * The read-only bit follows the file name, not the file itself.
 */
struct _fref {
	CIRCLEQ_ENTRY(_fref) q;		/* Linked list of file references. */
	char	*name;			/* File name. */
	char	*tname;			/* Backing temporary file name. */

	db_recno_t	 lno;			/* 1-N: file cursor line. */
	size_t	 cno;			/* 0-N: file cursor column. */

#define	FR_CURSORSET	0x0001		/* If lno/cno values valid. */
#define	FR_DONTDELETE	0x0002		/* Don't delete the temporary file. */
#define	FR_EXNAMED	0x0004		/* Read/write renamed the file. */
#define	FR_NAMECHANGE	0x0008		/* If the name changed. */
#define	FR_NEWFILE	0x0010		/* File doesn't really exist yet. */
#define	FR_RECOVER	0x0020		/* File is being recovered. */
#define	FR_TMPEXIT	0x0040		/* Modified temporary file, no exit. */
#define	FR_TMPFILE	0x0080		/* If file has no name. */
#define	FR_UNLOCKED	0x0100		/* File couldn't be locked. */
	u_int16_t flags;
};

/* Action arguments to scr_exadjust(). */
typedef enum { EX_TERM_CE, EX_TERM_SCROLL } exadj_t;

/* Screen attribute arguments to scr_attr(). */
typedef enum { SA_ALTERNATE, SA_INVERSE } scr_attr_t;

/* Key type arguments to scr_keyval(). */
typedef enum { KEY_VEOF, KEY_VERASE, KEY_VKILL, KEY_VWERASE } scr_keyval_t;

/*
 * GS:
 *
 * Structure that describes global state of the running program.
 */
struct _gs {
	char	*progname;		/* Programe name. */

	int	 id;			/* Last allocated screen id. */
	CIRCLEQ_HEAD(_dqh, _win) dq;	/* Displayed windows. */
	CIRCLEQ_HEAD(_hqh, _scr) hq;	/* Hidden screens. */

	void	*perl_interp;		/* Perl interpreter. */
	void	*tcl_interp;		/* Tcl_Interp *: Tcl interpreter. */

	void	*cl_private;		/* Curses support private area. */
	void	*tk_private;		/* Tk/Tcl support private area. */

					/* File references. */
	CIRCLEQ_HEAD(_frefh, _fref) frefq;
 					/* File structures. */
 	CIRCLEQ_HEAD(_exfh, _exf) exfq;

#define	GO_COLUMNS	0		/* Global options: columns. */
#define	GO_LINES	1		/* Global options: lines. */
#define	GO_SECURE	2		/* Global options: secure. */
#define	GO_TERM		3		/* Global options: terminal type. */
	OPTION	 opts[GO_TERM + 1];

	DB	*msg;			/* Message catalog DB. */
	MSGH	 msgq;			/* User message list. */
#define	DEFAULT_NOPRINT	'\1'		/* Emergency non-printable character. */
	int	 noprint;		/* Cached, unprintable character. */

	char	*c_option;		/* Ex initial, command-line command. */

#ifdef DEBUG
	FILE	*tracefp;		/* Trace file pointer. */
#endif

#define	MAX_BIT_SEQ	0x7f		/* Max + 1 fast check character. */
	LIST_HEAD(_seqh, _seq) seqq;	/* Linked list of maps, abbrevs. */
	bitstr_t bit_decl(seqb, MAX_BIT_SEQ + 1);

#define	MAX_FAST_KEY	0xff		/* Max fast check character.*/
#define	KEY_LEN(sp, ch)							\
	(((ch) & ~MAX_FAST_KEY) == 0 ?					\
	    sp->gp->cname[(unsigned char)ch].len : v_key_len(sp, ch))
#define	KEY_NAME(sp, ch)						\
	(((ch) & ~MAX_FAST_KEY) == 0 ?					\
	    sp->gp->cname[(unsigned char)ch].name : v_key_name(sp, ch))
	struct {
		u_char	 name[MAX_CHARACTER_COLUMNS + 1];
		u_int8_t len;
	} cname[MAX_FAST_KEY + 1];	/* Fast lookup table. */

#define	KEY_VAL(sp, ch)							\
	(((ch) & ~MAX_FAST_KEY) == 0 ? 					\
	    sp->gp->special_key[(unsigned char)ch] : v_key_val(sp,ch))
	e_key_t				/* Fast lookup table. */
	    special_key[MAX_FAST_KEY + 1];

/* Flags. */
#define	G_ABBREV	0x0001		/* If have abbreviations. */
#define	G_BELLSCHED	0x0002		/* Bell scheduled. */
#define	G_INTERRUPTED	0x0004		/* Interrupted. */
#define	G_RECOVER_SET	0x0008		/* Recover system initialized. */
#define	G_SCRIPTED	0x0010		/* Ex script session. */
#define	G_SCRWIN	0x0020		/* Scripting windows running. */
#define	G_SNAPSHOT	0x0040		/* Always snapshot files. */
#define	G_SRESTART	0x0080		/* Screen restarted. */
	u_int32_t flags;

	/* Screen interface functions. */
					/* Add a string to the screen. */
	int	(*scr_addstr) __P((SCR *, const char *, size_t));
					/* Add a string to the screen. */
	int	(*scr_waddstr) __P((SCR *, const CHAR_T *, size_t));
					/* Toggle a screen attribute. */
	int	(*scr_attr) __P((SCR *, scr_attr_t, int));
					/* Terminal baud rate. */
	int	(*scr_baud) __P((SCR *, u_long *));
					/* Beep/bell/flash the terminal. */
	int	(*scr_bell) __P((SCR *));
					/* Display a busy message. */
	void	(*scr_busy) __P((SCR *, const char *, busy_t));
					/* Prepare child. */
	int	(*scr_child) __P((SCR *));
					/* Clear to the end of the line. */
	int	(*scr_clrtoeol) __P((SCR *));
					/* Return the cursor location. */
	int	(*scr_cursor) __P((SCR *, size_t *, size_t *));
					/* Delete a line. */
	int	(*scr_deleteln) __P((SCR *));
					/* Discard a screen. */
	int	(*scr_discard) __P((SCR *, SCR **));
					/* Get a keyboard event. */
	int	(*scr_event) __P((SCR *, EVENT *, u_int32_t, int));
					/* Ex: screen adjustment routine. */
	int	(*scr_ex_adjust) __P((SCR *, exadj_t));
	int	(*scr_fmap)		/* Set a function key. */
	    __P((SCR *, seq_t, CHAR_T *, size_t, CHAR_T *, size_t));
					/* Get terminal key value. */
	int	(*scr_keyval) __P((SCR *, scr_keyval_t, CHAR_T *, int *));
					/* Insert a line. */
	int	(*scr_insertln) __P((SCR *));
					/* Handle an option change. */
	int	(*scr_optchange) __P((SCR *, int, const char *, u_long *));
					/* Move the cursor. */
	int	(*scr_move) __P((SCR *, size_t, size_t));
					/* Refresh the screen. */
	int	(*scr_refresh) __P((SCR *, int));
					/* Rename the file. */
	int	(*scr_rename) __P((SCR *, char *, int));
					/* Reply to an event. */
	int	(*scr_reply) __P((SCR *, int, char *));
					/* Set the screen type. */
	int	(*scr_screen) __P((SCR *, u_int32_t));
					/* Split the screen. */
	int	(*scr_split) __P((SCR *, SCR *));
					/* Suspend the editor. */
	int	(*scr_suspend) __P((SCR *, int *));
					/* Print usage message. */
	void	(*scr_usage) __P((void));

	/* Threading stuff */
	void	*th_private;

	int	(*run) __P((WIN *, void *(*)(void*), void *));

	int	(*lock_init) __P((WIN *, void **));
#define LOCK_INIT(wp,s)							    \
	wp->gp->lock_init(wp, &s->lock)
	int	(*lock_try) __P((WIN *, void **));
#define LOCK_TRY(wp,s)							    \
	wp->gp->lock_try(wp, &s->lock)
	int	(*lock_unlock) __P((WIN *, void **));
#define LOCK_UNLOCK(wp,s)						    \
	wp->gp->lock_unlock(wp, &s->lock)
	int	(*lock_end) __P((WIN *, void **));
#define LOCK_END(wp,s)							    \
	wp->gp->lock_end(wp, &s->lock)
};

/*
 * XXX
 * Block signals if there are asynchronous events.  Used to keep DB system calls
 * from being interrupted and not restarted, as that will result in consistency
 * problems.  This should be handled by DB.
 */
#ifdef BLOCK_SIGNALS
#include <signal.h>
extern sigset_t	__sigblockset;
#define	SIGBLOCK \
	(void)sigprocmask(SIG_BLOCK, &__sigblockset, NULL)
#define	SIGUNBLOCK \
	(void)sigprocmask(SIG_UNBLOCK, &__sigblockset, NULL);
#else
#define	SIGBLOCK
#define	SIGUNBLOCK
#endif
