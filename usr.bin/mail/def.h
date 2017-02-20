/*	$NetBSD: def.h,v 1.28 2014/10/18 08:33:30 snj Exp $	*/
/*
 * Copyright (c) 1980, 1993
 *	The Regents of the University of California.  All rights reserved.
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
 *
 *	@(#)def.h	8.4 (Berkeley) 4/20/95
 *	$NetBSD: def.h,v 1.28 2014/10/18 08:33:30 snj Exp $
 */

/*
 * Mail -- a mail program
 *
 * Author: Kurt Shoens (UCB) March 25, 1978
 */

#ifndef __DEF_H__
#define __DEF_H__

#include <sys/types.h>
#include <sys/file.h>
#include <sys/ioctl.h>
#include <sys/stat.h>
#include <sys/param.h>
#include <sys/time.h>
#include <sys/wait.h>

#include <ctype.h>
#include <err.h>
#include <errno.h>
#include <fcntl.h>
#include <paths.h>
#include <pwd.h>
#include <setjmp.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <termios.h>
#include <time.h>
#include <unistd.h>
#include <vis.h>

#include "pathnames.h"

#define	APPEND				/* New mail goes to end of mailbox */

#define COMMENT_CHAR	'#'		/* Comment character when sourcing */
#define	ESCAPE		'~'		/* Default escape for sending */
#define	NMLSIZE		1024		/* max names in a message list */
#define	PATHSIZE	MAXPATHLEN	/* Size of pathnames throughout */
#define	HSHSIZE		59		/* Hash size for aliases and vars */
#define	LINESIZE	BUFSIZ		/* max readable line width */
#define	MAXARGC		1024		/* Maximum list of raw strings */
#define	MAXEXP		25		/* Maximum expansion of aliases */

#define PUBLIC			/* make it easy to find the entry points */

/*
 * User environment variable names.
 * See complete.h, mime.h, and thread.h for names specific to those modules.
 */
#define	ENAME_INDENT_POSTSCRIPT	"indentpostscript"
#define	ENAME_INDENT_PREAMBLE	"indentpreamble"
#define ENAME_APPEND		"append"
#define ENAME_ASK		"ask"
#define ENAME_ASKBCC		"askbcc"
#define ENAME_ASKCC		"askcc"
#define ENAME_ASKSUB		"asksub"
#define ENAME_AUTOINC		"autoinc"
#define ENAME_AUTOPRINT		"autoprint"
#define ENAME_CRT		"crt"
#define ENAME_DEAD		"DEAD"
#define ENAME_DEBUG		"debug"
#define ENAME_DONTSENDEMPTY	"dontsendempty"
#define ENAME_DOT		"dot"
#define ENAME_EDITOR		"EDITOR"
#define ENAME_ENABLE_PIPES	"enable-pipes"
#define ENAME_ESCAPE		"escape"
#define ENAME_FOLDER		"folder"
#define ENAME_HEADER_FORMAT	"header-format"
#define ENAME_HOLD		"hold"
#define ENAME_IGNORE		"ignore"
#define ENAME_IGNOREEOF		"ignoreeof"
#define ENAME_INDENTPREFIX	"indentprefix"
#define ENAME_INTERACTIVE	"interactive"
#define ENAME_KEEP		"keep"
#define ENAME_KEEPSAVE		"keepsave"
#define ENAME_LISTER		"LISTER"
#define ENAME_MBOX		"MBOX"
#define ENAME_METOO		"metoo"
#define ENAME_NOHEADER		"noheader"
#define ENAME_NOSAVE		"nosave"
#define ENAME_PAGE_ALSO		"page-also"
#define ENAME_PAGER		"PAGER"
#define ENAME_PAGER_OFF		"pager-off"
#define ENAME_PROMPT		"prompt"
#define ENAME_QUIET		"quiet"
#define ENAME_RECORD		"record"
#define ENAME_REGEX_SEARCH	"regex-search"
#define ENAME_REPLYALL		"Replyall"
#define ENAME_REPLYASRECIPIENT	"ReplyAsRecipient"
#define ENAME_SCREEN		"screen"
#define ENAME_SCREENHEIGHT	"screenheight"
#define ENAME_SCREENWIDTH	"screenwidth"
#define ENAME_SEARCHHEADERS	"searchheaders"
#define ENAME_SENDMAIL		"sendmail"
#define ENAME_SHELL		"SHELL"
#define ENAME_SHOW_RCPT		"show-rcpt"
#define ENAME_SMOPTS_VERIFY	"smopts-verify"
#define ENAME_TOPLINES		"toplines"
#define ENAME_VERBOSE		"verbose"
#define ENAME_VISUAL		"VISUAL"

#define	equal(a, b)	(strcmp(a,b)==0)/* A nice function to string compare */

struct message {
	short	m_flag;			/* flags, see below */
	short	m_offset;		/* offset in block of message */
	long	m_block;		/* block number of this message */
	long	m_lines;		/* Lines in the message */
	off_t	m_size;			/* Bytes in the message */
	long	m_blines;		/* Body (non-header) lines */

	/*
	 * threading fields
	 */
	int		m_index;	/* message index in this thread */
	int		m_depth;	/* depth in thread */
	struct message *m_flink;	/* link to next message */
	struct message *m_blink;	/* link to previous message */
	struct message *m_clink;	/* link to child of this message */
	struct message *m_plink;	/* link to parent of thread */
};
typedef struct mime_info mime_info_t;	/* phantom structure only to attach.c */

/*
 * flag bits.
 */

#define	MUSED		(1<<0)		/* entry is used, but this bit isn't */
#define	MDELETED	(1<<1)		/* entry has been deleted */
#define	MSAVED		(1<<2)		/* entry has been saved */
#define	MTOUCH		(1<<3)		/* entry has been noticed */
#define	MPRESERVE	(1<<4)		/* keep entry in sys mailbox */
#define	MMARK		(1<<5)		/* message is marked! */
#define	MMODIFY		(1<<6)		/* message has been modified */
#define	MNEW		(1<<7)		/* message has never been seen */
#define	MREAD		(1<<8)		/* message has been read sometime. */
#define	MSTATUS		(1<<9)		/* message status has changed */
#define	MBOX		(1<<10)		/* Send this to mbox, regardless */
#define MTAGGED		(1<<11)		/* message has been tagged */

/*
 * Given a file address, determine the block number it represents.
 */
#define blockof(off)			((int) ((off) / 4096))
#define blkoffsetof(off)		((int) ((off) % 4096))
#define positionof(block, offset)	((off_t)(block) * 4096 + (offset))

/*
 * Format of the command description table.
 * The actual table is declared and initialized
 * in lex.c
 */
struct cmd {
	const char *c_name;		/* Name of command */
	int	(*c_func)(void *);	/* Implementor of the command */
	int	c_pipe;			/* Pipe output through the pager */
# define C_PIPE_PAGER	1		/* enable use of pager */
# define C_PIPE_CRT	2		/* use the pager if CRT is defined */
# define C_PIPE_SHELL	4		/* enable shell pipes */
#ifdef USE_EDITLINE
	const char *c_complete;		/* String describing completion */
#endif
	short	c_argtype;		/* Type of arglist (see below) */
	short	c_msgflag;		/* Required flags of messages */
	short	c_msgmask;		/* Relevant flags of messages */
};

/* Yechh, can't initialize unions */

#define	c_minargs c_msgflag		/* Minimum argcount for RAWLIST */
#define	c_maxargs c_msgmask		/* Max argcount for RAWLIST */

/*
 * Argument types.
 */

#define	MSGLIST	 0		/* Message list type */
#define	STRLIST	 1		/* A pure string */
#define	RAWLIST	 2		/* Shell string list */
#define	NOLIST	 3		/* Just plain 0 */
#define	NDMLIST	 4		/* Message list, no defaults */

#define	P	0x010		/* Autoprint dot after command */
#define	I	0x020		/* Interactive command bit */
#define	M	0x040		/* Legal from send mode bit */
#define	W	0x080		/* Illegal when read only bit */
#define	F	0x100		/* Is a conditional command */
#define	T	0x200		/* Is a transparent command */
#define	R	0x400		/* Cannot be called from collect */
#define ARGTYPE_MASK	~(P|I|M|W|F|T|R)

/*
 * Oft-used mask values
 */

#define	MMNORM		(MDELETED|MSAVED)/* Look at both save and delete bits */
#define	MMNDEL		MDELETED	/* Look only at deleted bit */

/*
 * Structure used to return a break down of a head
 * line (hats off to Bill Joy!)
 */

struct headline {
	char	*l_from;	/* The name of the sender */
	char	*l_tty;		/* His tty string (if any) */
	char	*l_date;	/* The entire date string */
};

#define	GTO	 0x001		/* Grab To: line */
#define	GSUBJECT 0x002		/* Likewise, Subject: line */
#define	GCC	 0x004		/* And the Cc: line */
#define	GBCC	 0x008		/* And also the Bcc: line */
#define GSMOPTS  0x010		/* Grab the sendmail options */
#define GMISC	 0x020		/* miscellaneous extra fields for sending */
#ifdef MIME_SUPPORT
#define GMIME    0x040		/* mime flag */
#endif
#define	GMASK	(GTO | GSUBJECT | GCC | GBCC | GSMOPTS)
				/* Mask of places from whence */

#define	GNL	 0x100		/* Print blank line after */
#define	GDEL	 0x200		/* Entity removed from list */
#define	GCOMMA	 0x400		/* detract puts in commas */

#ifdef MIME_SUPPORT
/*
 * Structure of MIME content.
 */
struct Content {
	const char *C_type;		/* content type */
	const char *C_encoding;		/* content transfer encoding */
	const char *C_disposition;	/* content disposition */
	const char *C_description;	/* content description */
	const char *C_id;		/* content id */
};
/* Header strings corresponding to the above Content fields. */
#define MIME_HDR_TYPE		"Content-Type"
#define MIME_HDR_ENCODING	"Content-Transfer-Encoding"
#define MIME_HDR_DISPOSITION	"Content-Disposition"
#define MIME_HDR_ID		"Content-ID"
#define MIME_HDR_DESCRIPTION	"Content-Description"
#define MIME_HDR_VERSION	"MIME-Version"
/* the value of the MIME-Version field */
#define MIME_VERSION		"1.0"

typedef enum {
	ATTACH_INVALID = 0,	/* do not use! */
	ATTACH_FNAME = 1,
	ATTACH_MSG = 2,
	ATTACH_FILENO = 3
} attach_t;

/*
 * Structure of a MIME attachment.
 */
struct attachment {
	struct attachment *a_flink;	/* Forward link in list. */
	struct attachment *a_blink;	/* Backward list link */

	attach_t a_type;		/* attachment type */
#if 1
	union {
		char *u_name;		/* file name */
		struct message *u_msg;	/* message */
		int u_fileno;		/* file number */
	} a_u;

	#define a_name		a_u.u_name
	#define a_msg		a_u.u_msg
	#define a_fileno	a_u.u_fileno
#else
	char *a_name;			/* file name */
	struct message *a_msg;		/* message */
	int a_fileno;			/* file number */
#endif

	struct Content a_Content;	/* MIME content strings */
};
#endif /* MIME_SUPPORT */

/*
 * Structure used to pass about the current
 * state of the user-typed message header.
 */

struct header {
	struct name	*h_to;		/* Dynamic "To:" string */
	char		*h_subject;	/* Subject string */
	struct name	*h_cc;		/* Carbon copies string */
	struct name	*h_bcc;		/* Blind carbon copies */
	struct name	*h_smopts;	/* Sendmail options */
	char		*h_in_reply_to;	/* In-Reply-To: field */
	struct name	*h_references;	/* References: field */
	struct name	*h_extra;	/* extra header fields */
#ifdef MIME_SUPPORT
	char *h_mime_boundary;		/* MIME multipart boundary string */
	struct Content h_Content;	/* MIME content for message */
	struct attachment *h_attach;	/* MIME attachments */
#endif
};

/*
 * Structure of namelist nodes used in processing
 * the recipients of mail and aliases and all that
 * kind of stuff.
 */

struct name {
	struct	name *n_flink;		/* Forward link in list. */
	struct	name *n_blink;		/* Backward list link */
	short	n_type;			/* From which list it came */
	char	*n_name;		/* This fella's name */
};

/*
 * Structure of a variable node.  All variables are
 * kept on a singly-linked list of these, rooted by
 * "variables"
 */

struct var {
	struct	var *v_link;		/* Forward link to next variable */
	char	*v_name;		/* The variable's name */
	char	*v_value;		/* And its current value */
};

struct group {
	struct	group *ge_link;		/* Next person in this group */
	char	*ge_name;		/* This person's user name */
};

struct grouphead {
	struct	grouphead *g_link;	/* Next grouphead in list */
	char	*g_name;		/* Name of this group */
	struct	group *g_list;		/* Users in group. */
};

struct smopts_s {
	struct smopts_s *s_link;	/* Link to next smopts_s in list */
	char *s_name;			/* Name of this smopts_s */
	struct name *s_smopts;		/* sendmail options name list */
};

/*
 * Structure of the hash table of ignored header fields
 */
struct ignoretab {
	size_t i_count;			/* Number of entries */
	struct ignore {
		struct ignore *i_link;	/* Next ignored field in bucket */
		char *i_field;		/* This ignored field */
	} *i_head[HSHSIZE];
};

/*
 * Constants for conditional commands.  These control whether we
 * should be executing commands or not.
 */
struct cond_stack_s {
	struct cond_stack_s *c_next;
	int c_cond;
};
#define	CNONE		0x00		/* Execute everything */
#define	CSKIP		0x01		/* Do not execute commands */
#define	CIF		0x02		/* Inside if/endif block */
#define	CELSE		0x04		/* The last conditional was else */
#define	CIGN		0x08		/* Conditional in a skipped block */

enum mailmode_e {
	mm_receiving,			/* receiving mail mode */
	mm_sending,			/* sending mail mode */
	mm_hdrsonly			/* headers only mode */
};

/*
 * Truncate a file to the last character written. This is
 * useful just before closing an old file that was opened
 * for read/write.
 */
#define trunc(stream) {							\
	(void)fflush(stream); 						\
	(void)ftruncate(fileno(stream), (off_t)ftell(stream));		\
}

/*
 * White Space (WSP) as specified in see RFC 2822.
 *
 * NOTE: Use this in place of isblank() so it is inline.  Also, unlike
 * the table implemented ctype(3) routines, this does not have input
 * range issues caused by sign extensions.
 *
 * See mime_header.h for the related is_FWS().
 */
static inline int
is_WSP(int c)
{
	return c == ' ' || c == '\t';
}

static inline char *
skip_WSP(const char *cp)
{
	while (is_WSP(*cp))
		cp++;
	return __UNCONST(cp);
}

static inline char *
skip_space(char *p)
{
	while (isspace((unsigned char)*p))
		p++;
	return p;
}

/*
 * strip trailing white space
 */
static inline char *
strip_WSP(char *line)
{
	char *cp;

	cp = line + strlen(line) - 1;
	while (cp >= line && is_WSP(*cp))
		cp--;
	*++cp = '\0';
	return cp;
}

#endif /* __DEF_H__ */
