/*	$NetBSD: common.h,v 1.3 2012/01/27 16:41:22 christos Exp $ */

/*-
 * Copyright (c) 1991, 1993, 1994
 *	The Regents of the University of California.  All rights reserved.
 * Copyright (c) 1991, 1993, 1994, 1995, 1996
 *	Keith Bostic.  All rights reserved.
 *
 * See the LICENSE file for redistribution information.
 *
 *	Id: common.h,v 10.20 2002/03/02 23:36:22 skimo Exp (Berkeley) Date: 2002/03/02 23:36:22
 */

/*
 * Avoid include sys/types.h after definition of pgno_t
 */
#include <sys/types.h>
#include <sys/queue.h>
#include <bitstring.h>
/*
 * Porting information built at configuration time.  Included before
 * any of nvi's include files.
 */
#include "port.h"

/*
 * Pseudo-local includes.  These are files that are unlikely to exist
 * on most machines to which we're porting vi, and we want to include
 * them in a very specific order, regardless.
 */
#include "db.h"
#include <regex.h>

/*
 * Forward structure declarations.  Not pretty, but the include files
 * are far too interrelated for a clean solution.
 */
typedef struct _cb		CB;
typedef struct _csc		CSC;
typedef struct _conv	    	CONV;
typedef struct _conv_win    	CONVWIN;
typedef struct _event		EVENT;
typedef struct _excmd		EXCMD;
typedef struct _exf		EXF;
typedef struct _fref		FREF;
typedef struct _gs		GS;
typedef struct _lmark		LMARK;
typedef struct _mark		MARK;
typedef struct _msg		MSGS;
typedef struct _option		OPTION;
typedef struct _optlist		OPTLIST;
typedef struct _scr		SCR;
typedef struct _script		SCRIPT;
typedef struct _seq		SEQ;
typedef struct _tag		TAG;
typedef struct _tagf		TAGF;
typedef struct _tagq		TAGQ;
typedef struct _text		TEXT;
typedef struct _win		WIN;

/* Autoindent state. */
typedef enum { C_NOTSET, C_CARATSET, C_NOCHANGE, C_ZEROSET } carat_t;

/* Busy message types. */
typedef enum { BUSY_ON = 1, BUSY_OFF, BUSY_UPDATE } busy_t;

/*
 * Routines that return a confirmation return:
 *
 *	CONF_NO		User answered no.
 *	CONF_QUIT	User answered quit, eof or an error.
 *	CONF_YES	User answered yes.
 */
typedef enum { CONF_NO, CONF_QUIT, CONF_YES } conf_t;

/* Directions. */
typedef enum { NOTSET, FORWARD, BACKWARD } dir_t;

/* Line operations. */
typedef enum { LINE_APPEND, LINE_DELETE, LINE_INSERT, LINE_RESET } lnop_t;

/* Lock return values. */
typedef enum { LOCK_FAILED, LOCK_SUCCESS, LOCK_UNAVAIL } lockr_t;

/* Sequence types. */
typedef enum { SEQ_ABBREV, SEQ_COMMAND, SEQ_INPUT } seq_t;

#define	ENTIRE_LINE	((size_t)-1)
/*
 * Local includes.
 */
#include "key.h"		/* Required by args.h. */
#include "args.h"		/* Required by options.h. */
#include "options.h"		/* Required by screen.h. */

#include "msg.h"		/* Required by gs.h. */
#include "cut.h"		/* Required by gs.h. */
#include "seq.h"		/* Required by screen.h. */
#include "util.h"		/* Required by ex.h. */
#include "mark.h"		/* Required by gs.h. */
#include "conv.h"		/* Required by ex.h and screen.h */
#include "../ex/ex.h"		/* Required by gs.h. */
#include "gs.h"			/* Required by screen.h. */
#include "log.h"		/* Required by screen.h */
#include "screen.h"		/* Required by exf.h. */
#include "exf.h"
#include "mem.h"
#if defined(USE_DB4_LOGGING)
#include "vi_auto.h"
#endif

#include "extern.h"
