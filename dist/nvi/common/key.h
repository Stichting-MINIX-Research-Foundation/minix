/*	$NetBSD: key.h,v 1.3 2011/11/14 13:29:07 tnozaki Exp $ */

/*-
 * Copyright (c) 1991, 1993, 1994
 *	The Regents of the University of California.  All rights reserved.
 * Copyright (c) 1991, 1993, 1994, 1995, 1996
 *	Keith Bostic.  All rights reserved.
 *
 * See the LICENSE file for redistribution information.
 *
 *	Id: key.h,v 10.50 2001/06/28 17:53:58 skimo Exp (Berkeley) Date: 2001/06/28 17:53:58
 */

#include "multibyte.h"

#ifdef USE_WIDECHAR
#define FILE2INT5(sp,buf,n,nlen,w,wlen)					    \
    sp->conv.file2int(sp, n, nlen, &buf, &wlen, &w)
#define INT2FILE(sp,w,wlen,n,nlen) 					    \
    sp->conv.int2file(sp, w, wlen, &sp->wp->cw, &nlen, &n)
#define CHAR2INT5(sp,buf,n,nlen,w,wlen)					    \
    sp->conv.sys2int(sp, n, nlen, &buf, &wlen, &w)
#define INT2CHAR(sp,w,wlen,n,nlen) 					    \
    sp->conv.int2sys(sp, w, wlen, &sp->wp->cw, &nlen, &n)
#define INT2SYS(sp,w,wlen,n,nlen) 					    \
    sp->conv.int2sys(sp, w, wlen, &sp->wp->cw, &nlen, &n)
#define INPUT2INT5(sp,cw,n,nlen,w,wlen)					    \
    sp->conv.input2int(sp, n, nlen, &(cw), &wlen, &w)
#define CONST
#define CHAR_WIDTH(sp, ch)  wcwidth(ch)
#define INTISWIDE(c)	(wctob(c) == EOF)	    /* XXX wrong name */
#else
#define FILE2INT5(sp,buf,n,nlen,w,wlen) \
    (w = n, wlen = nlen, 0)
#define INT2FILE(sp,w,wlen,n,nlen) \
    (n = w, nlen = wlen, 0)
#define CHAR2INT5(sp,buf,n,nlen,w,wlen) \
    (w = n, wlen = nlen, 0)
#define INT2CHAR(sp,w,wlen,n,nlen) \
    (n = w, nlen = wlen, 0)
#define INT2SYS(sp,w,wlen,n,nlen) \
    (n = w, nlen = wlen, 0)
#define INPUT2INT5(sp,buf,n,nlen,w,wlen) \
    (w = n, wlen = nlen, 0)
#define CONST const
#define INTISWIDE(c)	    0
#define CHAR_WIDTH(sp, ch)  1
#endif
#define FILE2INT(sp,n,nlen,w,wlen)					    \
    FILE2INT5(sp,sp->wp->cw,n,nlen,w,wlen)
#define CHAR2INT(sp,n,nlen,w,wlen)					    \
    CHAR2INT5(sp,sp->wp->cw,n,nlen,w,wlen)

#define MEMCPYW(to, from, n) \
    memcpy(to, from, (n) * sizeof(CHAR_T))
#define MEMMOVEW(to, from, n) \
    memmove(to, from, (n) * sizeof(CHAR_T))

/* The maximum number of columns any character can take up on a screen. */
#define	MAX_CHARACTER_COLUMNS	4

/*
 * Event types.
 *
 * The program structure depends on the event loop being able to return
 * E_EOF/E_ERR multiple times -- eventually enough things will end due
 * to the events that vi will reach the command level for the screen, at
 * which point the exit flags will be set and vi will exit.
 */
typedef enum {
	E_NOTUSED = 0,			/* Not set. */
	E_CHARACTER,			/* Input character: e_c set. */
	E_EOF,				/* End of input (NOT ^D). */
	E_ERR,				/* Input error. */
	E_INTERRUPT,			/* Interrupt. */
	E_IPCOMMAND,			/* IP command: e_ipcom set. */
	E_REPAINT,			/* Repaint: e_flno, e_tlno set. */
	E_SIGHUP,			/* SIGHUP. */
	E_SIGTERM,			/* SIGTERM. */
	E_STRING,			/* Input string: e_csp, e_len set. */
	E_TIMEOUT,			/* Timeout. */
	E_WRESIZE,			/* Window resize. */
	E_FLAGS				/* Flags */
} e_event_t;

/*
 * Character values.
 */
typedef enum {
	K_NOTUSED = 0,			/* Not set. */
	K_BACKSLASH,			/*  \ */
	K_CARAT,			/*  ^ */
	K_CNTRLD,			/* ^D */
	K_CNTRLR,			/* ^R */
	K_CNTRLT,			/* ^T */
	K_CNTRLZ,			/* ^Z */
	K_COLON,			/*  : */
	K_CR,				/* \r */
	K_ESCAPE,			/* ^[ */
	K_FORMFEED,			/* \f */
	K_HEXCHAR,			/* ^X */
	K_NL,				/* \n */
	K_RIGHTBRACE,			/*  } */
	K_RIGHTPAREN,			/*  ) */
	K_TAB,				/* \t */
	K_VERASE,			/* set from tty: default ^H */
	K_VKILL,			/* set from tty: default ^U */
	K_VLNEXT,			/* set from tty: default ^V */
	K_VWERASE,			/* set from tty: default ^W */
	K_ZERO				/*  0 */
} e_key_t;

struct _event {
	TAILQ_ENTRY(_event) q;		/* Linked list of events. */
	e_event_t e_event;		/* Event type. */
	int	  e_ipcom;		/* IP command. */

#define	CH_ABBREVIATED	0x01		/* Character is from an abbreviation. */
#define	CH_MAPPED	0x02		/* Character is from a map. */
#define	CH_NOMAP	0x04		/* Do not map the character. */
#define	CH_QUOTED	0x08		/* Character is already quoted. */
	ARG_CHAR_T e_c;			/* Character. */
	e_key_t	  e_value;		/* Key type. */

#define	e_flags	e_val1			/* Flags. */
#define	e_lno	e_val1			/* Single location. */
#define	e_cno	e_val2
#define	e_flno	e_val1			/* Text region. */
#define	e_fcno	e_val2
#define	e_tlno	e_val3
#define	e_tcno	e_val4
	size_t	  e_val1;		/* Value #1. */
	size_t	  e_val2;		/* Value #2. */
	size_t	  e_val3;		/* Value #3. */
	size_t	  e_val4;		/* Value #4. */

#define	e_csp	e_str1
#define	e_len	e_len1
	CHAR_T	 *e_str1;		/* String #1. */
	size_t	  e_len1;		/* String #1 length. */
	CHAR_T	 *e_str2;		/* String #2. */
	size_t	  e_len2;		/* String #2 length. */
};

typedef struct _keylist {
	e_key_t value;			/* Special value. */
	int	ch;			/* Key. */
} KEYLIST;
extern KEYLIST keylist[];

					/* Return if more keys in queue. */
#define	KEYS_WAITING(sp)	((sp)->wp->i_cnt != 0)
#define	MAPPED_KEYS_WAITING(sp)						\
	(KEYS_WAITING(sp) &&						\
	    FL_ISSET((sp)->wp->i_event[(sp)->wp->i_next].e_flags, CH_MAPPED))

/* The "standard" tab width, for displaying things to users. */
#define	STANDARD_TAB	6

/* Various special characters, messages. */
#define	CH_BSEARCH	'?'		/* Backward search prompt. */
#define	CH_CURSOR	' '		/* Cursor character. */
#define	CH_ENDMARK	'$'		/* End of a range. */
#define	CH_EXPROMPT	':'		/* Ex prompt. */
#define	CH_FSEARCH	'/'		/* Forward search prompt. */
#define	CH_HEX		'\030'		/* Leading hex character. */
#define	CH_LITERAL	'\026'		/* ASCII ^V. */
#define	CH_NO		'n'		/* No. */
#define	CH_NOT_DIGIT	'a'		/* A non-isdigit() character. */
#define	CH_QUIT		'q'		/* Quit. */
#define	CH_YES		'y'		/* Yes. */

/*
 * Checking for interrupts means that we look at the bit that gets set if the
 * screen code supports asynchronous events, and call back into the event code
 * so that non-asynchronous screens get a chance to post the interrupt.
 *
 * INTERRUPT_CHECK is the number of lines "operated" on before checking for
 * interrupts.
 */
#define	INTERRUPT_CHECK	100
#define	INTERRUPTED(sp)							\
	(F_ISSET((sp)->gp, G_INTERRUPTED) ||				\
	(!v_event_get(sp, NULL, 0, EC_INTERRUPT) &&			\
	F_ISSET((sp)->gp, G_INTERRUPTED)))
#define	CLR_INTERRUPT(sp)						\
	F_CLR((sp)->gp, G_INTERRUPTED)

/* Flags describing types of characters being requested. */
#define	EC_INTERRUPT	0x001		/* Checking for interrupts. */
#define	EC_MAPCOMMAND	0x002		/* Apply the command map. */
#define	EC_MAPINPUT	0x004		/* Apply the input map. */
#define	EC_MAPNODIGIT	0x008		/* Return to a digit. */
#define	EC_QUOTED	0x010		/* Try to quote next character */
#define	EC_RAW		0x020		/* Any next character. XXX: not used. */
#define	EC_TIMEOUT	0x040		/* Timeout to next character. */

/* Flags describing text input special cases. */
#define	TXT_ADDNEWLINE	0x00000001	/* Replay starts on a new line. */
#define	TXT_AICHARS	0x00000002	/* Leading autoindent chars. */
#define	TXT_ALTWERASE	0x00000004	/* Option: altwerase. */
#define	TXT_APPENDEOL	0x00000008	/* Appending after EOL. */
#define	TXT_AUTOINDENT	0x00000010	/* Autoindent set this line. */
#define	TXT_BACKSLASH	0x00000020	/* Backslashes escape characters. */
#define	TXT_BEAUTIFY	0x00000040	/* Only printable characters. */
#define	TXT_BS		0x00000080	/* Backspace returns the buffer. */
#define	TXT_CEDIT	0x00000100	/* Can return TERM_CEDIT. */
#define	TXT_CNTRLD	0x00000200	/* Control-D is a command. */
#define	TXT_CNTRLT	0x00000400	/* Control-T is an indent special. */
#define	TXT_CR		0x00000800	/* CR returns the buffer. */
#define	TXT_DOTTERM	0x00001000	/* Leading '.' terminates the input. */
#define	TXT_EMARK	0x00002000	/* End of replacement mark. */
#define	TXT_EOFCHAR	0x00004000	/* ICANON set, return EOF character. */
#define	TXT_ESCAPE	0x00008000	/* Escape returns the buffer. */
#define	TXT_FILEC	0x00010000	/* Option: filec. */
#define	TXT_INFOLINE	0x00020000	/* Editing the info line. */
#define	TXT_MAPINPUT	0x00040000	/* Apply the input map. */
#define	TXT_NLECHO	0x00080000	/* Echo the newline. */
#define	TXT_NUMBER	0x00100000	/* Number the line. */
#define	TXT_OVERWRITE	0x00200000	/* Overwrite characters. */
#define	TXT_PROMPT	0x00400000	/* Display a prompt. */
#define	TXT_RECORD	0x00800000	/* Record for replay. */
#define	TXT_REPLACE	0x01000000	/* Replace; don't delete overwrite. */
#define	TXT_REPLAY	0x02000000	/* Replay the last input. */
#define	TXT_RESOLVE	0x04000000	/* Resolve the text into the file. */
#define	TXT_SEARCHINCR	0x08000000	/* Incremental search. */
#define	TXT_SHOWMATCH	0x10000000	/* Option: showmatch. */
#define	TXT_TTYWERASE	0x20000000	/* Option: ttywerase. */
#define	TXT_WRAPMARGIN	0x40000000	/* Option: wrapmargin. */
