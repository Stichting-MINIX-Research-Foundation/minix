/*-
 * Copyright (c) 1992, 1993, 1994
 *	The Regents of the University of California.  All rights reserved.
 * Copyright (c) 1992, 1993, 1994, 1995, 1996
 *	Keith Bostic.  All rights reserved.
 * Copyright (c) 1995
 *	George V. Neville-Neil. All rights reserved.
 * Copyright (c) 1996-2001
 *	Sven Verdoolaege. All rights reserved.
 *
 * See the LICENSE file for redistribution information.
 */

#undef VI

#ifndef lint
static const char sccsid[] = "Id: perl.xs,v 8.46 2001/08/28 11:33:42 skimo Exp  (Berkeley) Date: 2001/08/28 11:33:42 ";
#endif /* not lint */

#include <sys/types.h>
#include <sys/queue.h>
#include <sys/time.h>

#include <bitstring.h>
#include <ctype.h>
#include <limits.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <termios.h>
#include <unistd.h>

#include <EXTERN.h>
#include <perl.h>
#include <XSUB.h>

/* perl redefines them
 * avoid warnings
 */
#undef USE_DYNAMIC_LOADING
#undef DEBUG
#undef PACKAGE
#undef ARGS
#define ARGS ARGS

#include "config.h"

#include "../common/common.h"
#include "perl_api_extern.h"

#ifndef DEFSV
#define DEFSV GvSV(defgv)
#endif
#ifndef ERRSV
#define ERRSV GvSV(errgv)
#endif
#ifndef dTHX
#define dTHXs
#else
#define dTHXs dTHX;
#endif

static void msghandler __P((SCR *, mtype_t, char *, size_t));

typedef struct _perl_data {
    	PerlInterpreter*	interp;
	SV 	*svcurscr, *svstart, *svstop, *svid;
	CONVWIN	 cw;
	char 	*errmsg;
} perl_data_t;

#define PERLP(sp)   ((perl_data_t *)sp->wp->perl_private)

#define CHAR2INTP(sp,n,nlen,w,wlen)					    \
    CHAR2INT5(sp,((perl_data_t *)sp->wp->perl_private)->cw,n,nlen,w,wlen)

/*
 * INITMESSAGE --
 *	Macros to point messages at the Perl message handler.
 */
#define	INITMESSAGE(sp)							\
	scr_msg = sp->wp->scr_msg;					\
	sp->wp->scr_msg = msghandler;
#define	ENDMESSAGE(sp)							\
	sp->wp->scr_msg = scr_msg;					\
	if (rval) croak(PERLP(sp)->errmsg);

void xs_init __P((pTHXo));

/*
 * perl_end --
 *	Clean up perl interpreter
 *
 * PUBLIC: int perl_end __P((GS *));
 */
int
perl_end(gp)
	GS *gp;
{
	/*
	 * Call perl_run and perl_destuct to call END blocks and DESTROY
	 * methods.
	 */
	if (gp->perl_interp) {
		perl_run(gp->perl_interp);
		perl_destruct(gp->perl_interp);
#if defined(DEBUG) || defined(PURIFY) || defined(LIBRARY)
		perl_free(gp->perl_interp);
#endif
		/* XXX rather make sure only one thread calls perl_end */
		gp->perl_interp = 0;
	}
}

/*
 * perl_eval
 *	Evaluate a string
 * 	We don't use mortal SVs because no one will clean up after us
 */
static void 
perl_eval(string)
	char *string;
{
	dTHXs

	SV* sv = newSVpv(string, 0);

	/* G_KEEPERR to catch syntax error; better way ? */
	sv_setpv(ERRSV,"");
	perl_eval_sv(sv, G_DISCARD | G_NOARGS | G_KEEPERR);
	SvREFCNT_dec(sv);
}

/*
 * perl_init --
 *	Create the perl commands used by nvi.
 *
 * PUBLIC: int perl_init __P((SCR *));
 */
int
perl_init(scrp)
	SCR *scrp;
{
	AV * av;
	GS *gp;
	WIN *wp;
	char *bootargs[] = { "VI", NULL };
#ifndef USE_SFIO
	SV *svcurscr;
#endif
	perl_data_t *pp;

	static char *args[] = { "", "-e", "" };
	size_t length;
	char *file = __FILE__;

	gp = scrp->gp;
	wp = scrp->wp;

	if (gp->perl_interp == NULL) {
	gp->perl_interp = perl_alloc();
  	perl_construct(gp->perl_interp);
	if (perl_parse(gp->perl_interp, xs_init, 3, args, 0)) {
		perl_destruct(gp->perl_interp);
		perl_free(gp->perl_interp);
		gp->perl_interp = NULL;
		return 1;
	}
	{
	dTHXs

        perl_call_argv("VI::bootstrap", G_DISCARD, bootargs);
	perl_eval("$SIG{__WARN__}='VI::Warn'");

	av_unshift(av = GvAVn(PL_incgv), 1);
	av_store(av, 0, newSVpv(_PATH_PERLSCRIPTS,
				sizeof(_PATH_PERLSCRIPTS)-1));

#ifdef USE_SFIO
	sfdisc(PerlIO_stdout(), sfdcnewnvi(scrp));
	sfdisc(PerlIO_stderr(), sfdcnewnvi(scrp));
#else
	svcurscr = perl_get_sv("curscr", TRUE);
	sv_magic((SV *)gv_fetchpv("STDOUT",TRUE, SVt_PVIO), svcurscr,
		 	'q', Nullch, 0);
	sv_magic((SV *)gv_fetchpv("STDERR",TRUE, SVt_PVIO), svcurscr,
		 	'q', Nullch, 0);
#endif /* USE_SFIO */
	}
	}
	MALLOC(scrp, pp, perl_data_t *, sizeof(perl_data_t));
	wp->perl_private = pp;
	memset(&pp->cw, 0, sizeof(pp->cw));
#ifdef USE_ITHREADS
	pp->interp = perl_clone(gp->perl_interp, 0);
        if (1) { /* hack for bug fixed in perl-current (5.6.1) */
            dTHXa(pp->interp);
            if (PL_scopestack_ix == 0) {
                ENTER;
            }
        }
#else
	pp->interp = gp->perl_interp;
#endif
	pp->errmsg = 0;
	{
		dTHXs

		SvREADONLY_on(pp->svcurscr = perl_get_sv("curscr", TRUE));
		SvREADONLY_on(pp->svstart = perl_get_sv("VI::StartLine", TRUE));
		SvREADONLY_on(pp->svstop = perl_get_sv("VI::StopLine", TRUE));
		SvREADONLY_on(pp->svid = perl_get_sv("VI::ScreenId", TRUE));
	}
	return (0);
}

/*
 * perl_screen_end
 *	Remove all refences to the screen to be destroyed
 *
 * PUBLIC: int perl_screen_end __P((SCR*));
 */
int
perl_screen_end(scrp)
	SCR *scrp;
{
	dTHXs

	if (scrp->perl_private) {
		sv_setiv((SV*) scrp->perl_private, 0);
	}
	return 0;
}

static void
my_sighandler(i)
	int i;
{
	croak("Perl command interrupted by SIGINT");
}

/* Create a new reference to an SV pointing to the SCR structure
 * The perl_private part of the SCR structure points to the SV,
 * so there can only be one such SV for a particular SCR structure.
 * When the last reference has gone (DESTROY is called),
 * perl_private is reset; When the screen goes away before
 * all references are gone, the value of the SV is reset;
 * any subsequent use of any of those reference will produce
 * a warning. (see typemap)
 */
static SV *
newVIrv(rv, screen)
	SV *rv;
	SCR *screen;
{
	dTHXs

	if (!screen) return sv_setsv(rv, &PL_sv_undef), rv;
	sv_upgrade(rv, SVt_RV);
	if (!screen->perl_private) {
		screen->perl_private = newSV(0);
		sv_setiv(screen->perl_private, (IV) screen);
	} 
	else SvREFCNT_inc(screen->perl_private);
	SvRV(rv) = screen->perl_private;
	SvROK_on(rv);
	return sv_bless(rv, gv_stashpv("VI", TRUE));
}

/*
 * perl_setenv
 *	Use perl's setenv if perl interpreter has been started.
 *	Perl uses its own setenv and gets confused if we change
 *	the environment after it has started.
 *
 * PUBLIC: int perl_setenv __P((SCR* sp, const char *name, const char *value));
 */
int
perl_setenv(SCR* scrp, const char *name, const char *value)
{
	if (scrp->wp->perl_private == NULL) {
	    if (value == NULL)
		unsetenv(name);
	    else
		setenv(name, value, 1);
	} else
	    my_setenv(name, value);
}


/* 
 * perl_ex_perl -- :[line [,line]] perl [command]
 *	Run a command through the perl interpreter.
 *
 * PUBLIC: int perl_ex_perl __P((SCR*, CHAR_T *, size_t, db_recno_t, db_recno_t));
 */
int 
perl_ex_perl(scrp, cmdp, cmdlen, f_lno, t_lno)
	SCR *scrp;
	CHAR_T *cmdp;
	size_t cmdlen;
	db_recno_t f_lno, t_lno;
{
	WIN *wp;
	size_t length;
	size_t len;
	char *err;
	char *np;
	size_t nlen;
	Signal_t (*istat)();
	perl_data_t *pp;

	/* Initialize the interpreter. */
	if (scrp->wp->perl_private == NULL && perl_init(scrp))
			return (1);
	pp = scrp->wp->perl_private;
    {
	dTHXs
	dSP;

	sv_setiv(pp->svstart, f_lno);
	sv_setiv(pp->svstop, t_lno);
	newVIrv(pp->svcurscr, scrp);
	/* Backwards compatibility. */
	newVIrv(pp->svid, scrp);

	istat = signal(SIGINT, my_sighandler);
	INT2CHAR(scrp, cmdp, STRLEN(cmdp)+1, np, nlen);
	perl_eval(np);
	signal(SIGINT, istat);

	SvREFCNT_dec(SvRV(pp->svcurscr));
	SvROK_off(pp->svcurscr);
	SvREFCNT_dec(SvRV(pp->svid));
	SvROK_off(pp->svid);

	err = SvPV(ERRSV, length);
	if (!length)
		return (0);

	err[length - 1] = '\0';
	msgq(scrp, M_ERR, "perl: %s", err);
	return (1);
    }
}

/*
 * replace_line
 *	replace a line with the contents of the perl variable $_
 *	lines are split at '\n's
 *	if $_ is undef, the line is deleted
 *	returns possibly adjusted linenumber
 */
static int 
replace_line(scrp, line, t_lno, defsv)
	SCR *scrp;
	db_recno_t line, *t_lno;
	SV *defsv;
{
	char *str, *next;
	CHAR_T *wp;
	size_t len, wlen;
	dTHXs

	if (SvOK(defsv)) {
		str = SvPV(defsv,len);
		next = memchr(str, '\n', len);
		CHAR2INTP(scrp, str, next ? (next - str) : len, wp, wlen);
		api_sline(scrp, line, wp, wlen);
		while (next++) {
			len -= next - str;
			next = memchr(str = next, '\n', len);
			CHAR2INTP(scrp, str, next ? (next - str) : len, 
				    wp, wlen);
			api_iline(scrp, ++line, wp, wlen);
			(*t_lno)++;
		}
	} else {
		api_dline(scrp, line--);
		(*t_lno)--;
	}
	return line;
}

/* 
 * perl_ex_perldo -- :[line [,line]] perl [command]
 *	Run a set of lines through the perl interpreter.
 *
 * PUBLIC: int perl_ex_perldo __P((SCR*, CHAR_T *, size_t, db_recno_t, db_recno_t));
 */
int 
perl_ex_perldo(scrp, cmdp, cmdlen, f_lno, t_lno)
	SCR *scrp;
	CHAR_T *cmdp;
	size_t cmdlen;
	db_recno_t f_lno, t_lno;
{
	CHAR_T *p;
	WIN *wp;
	size_t length;
	size_t len;
	db_recno_t i;
	CHAR_T *str;
	char *estr;
	SV* cv;
	char *command;
	perl_data_t *pp;
	char *np;
	size_t nlen;

	/* Initialize the interpreter. */
	if (scrp->wp->perl_private == NULL && perl_init(scrp))
			return (1);
	pp = scrp->wp->perl_private;
    {
	dTHXs
	dSP;

	newVIrv(pp->svcurscr, scrp);
	/* Backwards compatibility. */
	newVIrv(pp->svid, scrp);

	INT2CHAR(scrp, cmdp, STRLEN(cmdp)+1, np, nlen);
	if (!(command = malloc(length = nlen - 1 + sizeof("sub {}"))))
		return 1;
	snprintf(command, length, "sub {%s}", np);

	ENTER;
	SAVETMPS;

	cv = perl_eval_pv(command, FALSE);
	free (command);

	estr = SvPV(ERRSV,length);
	if (length)
		goto err;

	for (i = f_lno; i <= t_lno && !api_gline(scrp, i, &str, &len); i++) {
		INT2CHAR(scrp, str, len, np, nlen);
		sv_setpvn(DEFSV,np,nlen);
		sv_setiv(pp->svstart, i);
		sv_setiv(pp->svstop, i);
		PUSHMARK(sp);
                perl_call_sv(cv, G_SCALAR | G_EVAL);
		estr = SvPV(ERRSV, length);
		if (length) break;
		SPAGAIN;
		if(SvTRUEx(POPs)) 
			i = replace_line(scrp, i, &t_lno, DEFSV);
		PUTBACK;
	}
	FREETMPS;
	LEAVE;

	SvREFCNT_dec(SvRV(pp->svcurscr));
	SvROK_off(pp->svcurscr);
	SvREFCNT_dec(SvRV(pp->svid));
	SvROK_off(pp->svid);

	if (!length)
		return (0);

err:	estr[length - 1] = '\0';
	msgq(scrp, M_ERR, "perl: %s", estr);
	return (1);
    }
}

/*
 * msghandler --
 *	Perl message routine so that error messages are processed in
 *	Perl, not in nvi.
 */
static void
msghandler(sp, mtype, msg, len)
	SCR *sp;
	mtype_t mtype;
	char *msg;
	size_t len;
{
	char 	*errmsg;

	errmsg = PERLP(sp)->errmsg;

	/* Replace the trailing <newline> with an EOS. */
	/* Let's do that later instead */
	if (errmsg) free (errmsg);
	errmsg = malloc(len + 1);
	memcpy(errmsg, msg, len);
	errmsg[len] = '\0';
	PERLP(sp)->errmsg = errmsg;
}


typedef SCR *	VI;
typedef SCR *	VI__OPT;
typedef SCR *	VI__MAP;
typedef SCR * 	VI__MARK;
typedef SCR * 	VI__LINE;
typedef AV *	AVREF;

typedef struct {
    SV      *sprv;
    TAGQ    *tqp;
} perl_tagq;

typedef perl_tagq *  VI__TAGQ;
typedef perl_tagq *  VI__TAGQ2;

MODULE = VI	PACKAGE = VI

# msg --
#	Set the message line to text.
#
# Perl Command: VI::Msg
# Usage: VI::Msg screenId text

void
Msg(screen, text)
	VI          screen
	char *      text
 
	ALIAS:
	PRINT = 1

	CODE:
	api_imessage(screen, text);

# XS_VI_escreen --
#	End a screen.
#
# Perl Command: VI::EndScreen
# Usage: VI::EndScreen screenId

void
EndScreen(screen)
	VI	screen

	PREINIT:
	void (*scr_msg) __P((SCR *, mtype_t, char *, size_t));
	int rval;

	CODE:
	INITMESSAGE(screen);
	rval = api_escreen(screen);
	ENDMESSAGE(screen);

# XS_VI_iscreen --
#	Create a new screen.  If a filename is specified then the screen
#	is opened with that file.
#
# Perl Command: VI::NewScreen
# Usage: VI::NewScreen screenId [file]

VI
Edit(screen, ...)
	VI screen

	ALIAS:
	NewScreen = 1

	PROTOTYPE: $;$
	PREINIT:
	void (*scr_msg) __P((SCR *, mtype_t, char *, size_t));
	int rval;
	char *file;
	SCR *nsp;

	CODE:
	file = (items == 1) ? NULL : (char *)SvPV(ST(1),PL_na);
	INITMESSAGE(screen);
	rval = api_edit(screen, file, &nsp, ix);
	ENDMESSAGE(screen);
	
	RETVAL = ix ? nsp : screen;

	OUTPUT:
	RETVAL

# XS_VI_fscreen --
#	Return the screen id associated with file name.
#
# Perl Command: VI::FindScreen
# Usage: VI::FindScreen file

VI
FindScreen(file)
	char *file

	PREINIT:
	SCR *fsp;
	CODE:
	RETVAL = api_fscreen(0, file);

	OUTPUT:
	RETVAL

# XS_VI_GetFileName --
#	Return the file name of the screen
#
# Perl Command: VI::GetFileName
# Usage: VI::GetFileName screenId

char *
GetFileName(screen)
	VI screen;

	PPCODE:
	EXTEND(sp,1);
	PUSHs(sv_2mortal(newSVpv(screen->frp->name, 0)));

# XS_VI_aline --
#	-- Append the string text after the line in lineNumber.
#
# Perl Command: VI::AppendLine
# Usage: VI::AppendLine screenId lineNumber text

void
AppendLine(screen, linenumber, text)
	VI screen
	int linenumber
	char *text

	PREINIT:
	void (*scr_msg) __P((SCR *, mtype_t, char *, size_t));
	int rval;
	size_t length;

	CODE:
	SvPV(ST(2), length);
	INITMESSAGE(screen);
	rval = api_aline(screen, linenumber, text, length);
	ENDMESSAGE(screen);

# XS_VI_dline --
#	Delete lineNum.
#
# Perl Command: VI::DelLine
# Usage: VI::DelLine screenId lineNum

void 
DelLine(screen, linenumber)
	VI screen
	int linenumber

	PREINIT:
	void (*scr_msg) __P((SCR *, mtype_t, char *, size_t));
	int rval;

	CODE:
	INITMESSAGE(screen);
	rval = api_dline(screen, (db_recno_t)linenumber);
	ENDMESSAGE(screen);

# XS_VI_gline --
#	Return lineNumber.
#
# Perl Command: VI::GetLine
# Usage: VI::GetLine screenId lineNumber

char *
GetLine(screen, linenumber)
	VI screen
	int linenumber

	PREINIT:
	size_t len;
	void (*scr_msg) __P((SCR *, mtype_t, char *, size_t));
	int rval;
	char *line;
	CHAR_T *p;

	PPCODE:
	INITMESSAGE(screen);
	rval = api_gline(screen, (db_recno_t)linenumber, &p, &len);
	ENDMESSAGE(screen);

	EXTEND(sp,1);
        PUSHs(sv_2mortal(newSVpv(len ? (char *)p : "", len)));

# XS_VI_sline --
#	Set lineNumber to the text supplied.
#
# Perl Command: VI::SetLine
# Usage: VI::SetLine screenId lineNumber text

void
SetLine(screen, linenumber, text)
	VI screen
	int linenumber
	char *text

	PREINIT:
	void (*scr_msg) __P((SCR *, mtype_t, char *, size_t));
	int rval;
	size_t length;
	size_t len;
	CHAR_T *line;

	CODE:
	SvPV(ST(2), length);
	INITMESSAGE(screen);
	CHAR2INTP(screen, text, length, line, len);
	rval = api_sline(screen, linenumber, line, len);
	ENDMESSAGE(screen);

# XS_VI_iline --
#	Insert the string text before the line in lineNumber.
#
# Perl Command: VI::InsertLine
# Usage: VI::InsertLine screenId lineNumber text

void
InsertLine(screen, linenumber, text)
	VI screen
	int linenumber
	char *text

	PREINIT:
	void (*scr_msg) __P((SCR *, mtype_t, char *, size_t));
	int rval;
	size_t length;
	size_t len;
	CHAR_T *line;

	CODE:
	SvPV(ST(2), length);
	INITMESSAGE(screen);
	CHAR2INTP(screen, text, length, line, len);
	rval = api_iline(screen, linenumber, line, len);
	ENDMESSAGE(screen);

# XS_VI_lline --
#	Return the last line in the screen.
#
# Perl Command: VI::LastLine
# Usage: VI::LastLine screenId

int 
LastLine(screen)
	VI screen

	PREINIT:
	db_recno_t last;
	void (*scr_msg) __P((SCR *, mtype_t, char *, size_t));
	int rval;

	CODE:
	INITMESSAGE(screen);
	rval = api_lline(screen, &last);
	ENDMESSAGE(screen);
	RETVAL=last;

	OUTPUT:
	RETVAL

# XS_VI_getmark --
#	Return the mark's cursor position as a list with two elements.
#	{line, column}.
#
# Perl Command: VI::GetMark
# Usage: VI::GetMark screenId mark

void
GetMark(screen, mark)
	VI screen
	char mark

	PREINIT:
	struct _mark cursor;
	void (*scr_msg) __P((SCR *, mtype_t, char *, size_t));
	int rval;

	PPCODE:
	INITMESSAGE(screen);
	rval = api_getmark(screen, (int)mark, &cursor);
	ENDMESSAGE(screen);

	EXTEND(sp,2);
        PUSHs(sv_2mortal(newSViv(cursor.lno)));
        PUSHs(sv_2mortal(newSViv(cursor.cno)));

# XS_VI_setmark --
#	Set the mark to the line and column numbers supplied.
#
# Perl Command: VI::SetMark
# Usage: VI::SetMark screenId mark line column

void
SetMark(screen, mark, line, column)
	VI screen
	char mark
	int line
	int column

	PREINIT:
	struct _mark cursor;
	void (*scr_msg) __P((SCR *, mtype_t, char *, size_t));
	int rval;

	CODE:
	INITMESSAGE(screen);
	cursor.lno = line;
	cursor.cno = column;
	rval = api_setmark(screen, (int)mark, &cursor);
	ENDMESSAGE(screen);

# XS_VI_getcursor --
#	Return the current cursor position as a list with two elements.
#	{line, column}.
#
# Perl Command: VI::GetCursor
# Usage: VI::GetCursor screenId

void
GetCursor(screen)
	VI screen

	PREINIT:
	struct _mark cursor;
	void (*scr_msg) __P((SCR *, mtype_t, char *, size_t));
	int rval;

	PPCODE:
	INITMESSAGE(screen);
	rval = api_getcursor(screen, &cursor);
	ENDMESSAGE(screen);

	EXTEND(sp,2);
        PUSHs(sv_2mortal(newSViv(cursor.lno)));
        PUSHs(sv_2mortal(newSViv(cursor.cno)));

# XS_VI_setcursor --
#	Set the cursor to the line and column numbers supplied.
#
# Perl Command: VI::SetCursor
# Usage: VI::SetCursor screenId line column

void
SetCursor(screen, line, column)
	VI screen
	int line
	int column

	PREINIT:
	struct _mark cursor;
	void (*scr_msg) __P((SCR *, mtype_t, char *, size_t));
	int rval;

	CODE:
	INITMESSAGE(screen);
	cursor.lno = line;
	cursor.cno = column;
	rval = api_setcursor(screen, &cursor);
	ENDMESSAGE(screen);

# XS_VI_swscreen --
#	Change the current focus to screen.
#
# Perl Command: VI::SwitchScreen
# Usage: VI::SwitchScreen screenId screenId

void
SwitchScreen(screenFrom, screenTo)
	VI screenFrom
	VI screenTo

	PREINIT:
	void (*scr_msg) __P((SCR *, mtype_t, char *, size_t));
	int rval;

	CODE:
	INITMESSAGE(screenFrom);
	rval = api_swscreen(screenFrom, screenTo);
	ENDMESSAGE(screenFrom);

# XS_VI_map --
#	Associate a key with a perl procedure.
#
# Perl Command: VI::MapKey
# Usage: VI::MapKey screenId key perlproc

void
MapKey(screen, key, commandsv)
	VI screen
	char *key
	SV *commandsv

	PREINIT:
	void (*scr_msg) __P((SCR *, mtype_t, char *, size_t));
	int rval;
	int length;
	char *command;

	CODE:
	INITMESSAGE(screen);
	command = SvPV(commandsv, length);
	rval = api_map(screen, key, command, length);
	ENDMESSAGE(screen);

# XS_VI_unmap --
#	Unmap a key.
#
# Perl Command: VI::UnmapKey
# Usage: VI::UnmmapKey screenId key

void
UnmapKey(screen, key)
	VI screen
	char *key

	PREINIT:
	void (*scr_msg) __P((SCR *, mtype_t, char *, size_t));
	int rval;

	CODE:
	INITMESSAGE(screen);
	rval = api_unmap(screen, key);
	ENDMESSAGE(screen);

# XS_VI_opts_set --
#	Set an option.
#
# Perl Command: VI::SetOpt
# Usage: VI::SetOpt screenId setting

void
SetOpt(screen, setting)
	VI screen
	char *setting

	PREINIT:
	void (*scr_msg) __P((SCR *, mtype_t, char *, size_t));
	int rval;
	SV *svc;

	CODE:
	INITMESSAGE(screen);
	svc = sv_2mortal(newSVpv(":set ", 5));
	sv_catpv(svc, setting);
	rval = api_run_str(screen, SvPV(svc, PL_na));
	ENDMESSAGE(screen);

# XS_VI_opts_get --
#	Return the value of an option.
#	
# Perl Command: VI::GetOpt
# Usage: VI::GetOpt screenId option

void
GetOpt(screen, option)
	VI screen
	char *option

	PREINIT:
	void (*scr_msg) __P((SCR *, mtype_t, char *, size_t));
	int rval;
	char *value;
	CHAR_T *wp;
	size_t wlen;

	PPCODE:
	INITMESSAGE(screen);
	CHAR2INTP(screen, option, strlen(option)+1, wp, wlen);
	rval = api_opts_get(screen, wp, &value, NULL);
	ENDMESSAGE(screen);

	EXTEND(SP,1);
	PUSHs(sv_2mortal(newSVpv(value, 0)));
	free(value);

# XS_VI_run --
#	Run the ex command cmd.
#
# Perl Command: VI::Run
# Usage: VI::Run screenId cmd

void
Run(screen, command)
	VI screen
	char *command;

	PREINIT:
	void (*scr_msg) __P((SCR *, mtype_t, char *, size_t));
	int rval;

	CODE:
	INITMESSAGE(screen);
	rval = api_run_str(screen, command);
	ENDMESSAGE(screen);

void 
DESTROY(screensv)
	SV* screensv

	PREINIT:
	VI  screen;

	CODE:
	if (sv_isa(screensv, "VI")) {
		IV tmp = SvIV((SV*)SvRV(screensv));
		screen = (SCR *) tmp;
	}
	else
		croak("screen is not of type VI");

	if (screen)
	screen->perl_private = 0;

void
Warn(warning)
	char *warning;

	CODE:
	sv_catpv(ERRSV,warning);

#define TIED(kind,package) \
	sv_magic((SV *) (var = \
	    (kind##V *)sv_2mortal((SV *)new##kind##V())), \
		sv_setref_pv(sv_newmortal(), package, \
			newVIrv(newSV(0), screen)),\
		'P', Nullch, 0);\
	RETVAL = newRV((SV *)var)

SV *
Opt(screen)
	VI screen;
	PREINIT:
	HV *var;
	CODE:
	TIED(H,"VI::OPT");
	OUTPUT:
	RETVAL

SV *
Map(screen)
	VI screen;
	PREINIT:
	HV *var;
	CODE:
	TIED(H,"VI::MAP");
	OUTPUT:
	RETVAL

SV *
Mark(screen)
	VI screen
	PREINIT:
	HV *var;
	CODE:
	TIED(H,"VI::MARK");
	OUTPUT:
	RETVAL

SV *
Line(screen)
	VI screen
	PREINIT:
	AV *var;
	CODE:
	TIED(A,"VI::LINE");
	OUTPUT:
	RETVAL

SV *
TagQ(screen, tag)
	VI screen
	char *tag;

	PREINIT:
	perl_tagq *ptag;

	PPCODE:
	if ((ptag = malloc(sizeof(perl_tagq))) == NULL)
		goto err;

	ptag->sprv = newVIrv(newSV(0), screen);
	ptag->tqp = api_tagq_new(screen, tag);
	if (ptag->tqp != NULL) {
		EXTEND(SP,1);
		PUSHs(sv_2mortal(sv_setref_pv(newSV(0), "VI::TAGQ", ptag)));
	} else {
err:
		ST(0) = &PL_sv_undef;
		return;
	}

MODULE = VI	PACKAGE = VI::OPT

void 
DESTROY(screen)
	VI::OPT screen

	CODE:
	# typemap did all the checking
	SvREFCNT_dec((SV*)SvIV((SV*)SvRV(ST(0))));

void
FETCH(screen, key)
	VI::OPT screen
	char *key

	PREINIT:
	void (*scr_msg) __P((SCR *, mtype_t, char *, size_t));
	int rval;
	char *value;
	int boolvalue;
	CHAR_T *wp;
	size_t wlen;

	PPCODE:
	INITMESSAGE(screen);
	CHAR2INTP(screen, key, strlen(key)+1, wp, wlen);
	rval = api_opts_get(screen, wp, &value, &boolvalue);
	if (!rval) {
		EXTEND(SP,1);
		PUSHs(sv_2mortal((boolvalue == -1) ? newSVpv(value, 0)
						   : newSViv(boolvalue)));
		free(value);
	} else ST(0) = &PL_sv_undef;
	rval = 0;
	ENDMESSAGE(screen);

void
STORE(screen, key, value)
	VI::OPT	screen
	char	*key
	SV	*value

	PREINIT:
	void (*scr_msg) __P((SCR *, mtype_t, char *, size_t));
	int rval;
	CHAR_T *wp;
	size_t wlen;

	CODE:
	INITMESSAGE(screen);
	CHAR2INTP(screen, key, strlen(key)+1, wp, wlen);
	rval = api_opts_set(screen, wp, SvPV(value, PL_na), SvIV(value), 
                                         SvTRUEx(value));
	ENDMESSAGE(screen);

MODULE = VI	PACKAGE = VI::MAP

void 
DESTROY(screen)
	VI::MAP screen

	CODE:
	# typemap did all the checking
	SvREFCNT_dec((SV*)SvIV((SV*)SvRV(ST(0))));

void
STORE(screen, key, commandsv)
	VI::MAP screen
	char *key
	SV *commandsv

	PREINIT:
	void (*scr_msg) __P((SCR *, mtype_t, char *, size_t));
	int rval;
	int length;
	char *command;

	CODE:
	INITMESSAGE(screen);
	command = SvPV(commandsv, length);
	rval = api_map(screen, key, command, length);
	ENDMESSAGE(screen);

void
DELETE(screen, key)
	VI::MAP screen
	char *key

	PREINIT:
	void (*scr_msg) __P((SCR *, mtype_t, char *, size_t));
	int rval;

	CODE:
	INITMESSAGE(screen);
	rval = api_unmap(screen, key);
	ENDMESSAGE(screen);

MODULE = VI	PACKAGE = VI::MARK

void 
DESTROY(screen)
	VI::MARK screen

	CODE:
	# typemap did all the checking
	SvREFCNT_dec((SV*)SvIV((SV*)SvRV(ST(0))));

int
EXISTS(screen, mark)
	VI::MARK screen
	char mark

	PREINIT:
	struct _mark cursor;
	void (*scr_msg) __P((SCR *, mtype_t, char *, size_t));
	int rval = 0; /* never croak */
	int missing;

	CODE:
	INITMESSAGE(screen);
	missing = api_getmark(screen, (int)mark, &cursor);
	ENDMESSAGE(screen);
	RETVAL = !missing;

	OUTPUT:
	RETVAL

AV *
FETCH(screen, mark)
	VI::MARK screen
	char mark

	PREINIT:
	struct _mark cursor;
	void (*scr_msg) __P((SCR *, mtype_t, char *, size_t));
	int rval;

	CODE:
	INITMESSAGE(screen);
	rval = api_getmark(screen, (int)mark, &cursor);
	ENDMESSAGE(screen);
	RETVAL = newAV();
	av_push(RETVAL, newSViv(cursor.lno));
	av_push(RETVAL, newSViv(cursor.cno));

	OUTPUT:
	RETVAL

void
STORE(screen, mark, pos)
	VI::MARK screen
	char mark
	AVREF pos

	PREINIT:
	struct _mark cursor;
	void (*scr_msg) __P((SCR *, mtype_t, char *, size_t));
	int rval;

	CODE:
	if (av_len(pos) < 1) 
	    croak("cursor position needs 2 elements");
	INITMESSAGE(screen);
	cursor.lno = SvIV(*av_fetch(pos, 0, 0));
	cursor.cno = SvIV(*av_fetch(pos, 1, 0));
	rval = api_setmark(screen, (int)mark, &cursor);
	ENDMESSAGE(screen);

void
FIRSTKEY(screen, ...)
	VI::MARK screen

	ALIAS:
	NEXTKEY = 1
	
	PROTOTYPE: $;$

	PREINIT:
	int next;
	char key[] = {0, 0};

	PPCODE:
	if (items == 2) {
		next = 1;
		*key = *(char *)SvPV(ST(1),PL_na);
	} else next = 0;
	if (api_nextmark(screen, next, key) != 1) {
		EXTEND(sp, 1);
        	PUSHs(sv_2mortal(newSVpv(key, 1)));
	} else ST(0) = &PL_sv_undef;

MODULE = VI	PACKAGE = VI::LINE

void 
DESTROY(screen)
	VI::LINE screen

	CODE:
	# typemap did all the checking
	SvREFCNT_dec((SV*)SvIV((SV*)SvRV(ST(0))));

# similar to SetLine

void
STORE(screen, linenumber, text)
	VI::LINE screen
	int linenumber
	char *text

	PREINIT:
	void (*scr_msg) __P((SCR *, mtype_t, char *, size_t));
	int rval;
	size_t length;
	db_recno_t last;
	size_t len;
	CHAR_T *line;

	CODE:
	++linenumber;	/* vi 1 based ; perl 0 based */
	SvPV(ST(2), length);
	INITMESSAGE(screen);
	rval = api_lline(screen, &last);
	if (!rval) {
	    if (linenumber > last)
		rval = api_extend(screen, linenumber);
	    if (!rval)
		CHAR2INTP(screen, text, length, line, len);
		rval = api_sline(screen, linenumber, line, len);
	}
	ENDMESSAGE(screen);

# similar to GetLine 

char *
FETCH(screen, linenumber)
	VI::LINE screen
	int linenumber

	PREINIT:
	size_t len;
	void (*scr_msg) __P((SCR *, mtype_t, char *, size_t));
	int rval;
	char *line;
	CHAR_T *p;

	PPCODE:
	++linenumber;	/* vi 1 based ; perl 0 based */
	INITMESSAGE(screen);
	rval = api_gline(screen, (db_recno_t)linenumber, &p, &len);
	ENDMESSAGE(screen);

	EXTEND(sp,1);
	PUSHs(sv_2mortal(newSVpv(len ? (char*)p : "", len)));

# similar to LastLine 

int
FETCHSIZE(screen)
	VI::LINE screen

	PREINIT:
	db_recno_t last;
	void (*scr_msg) __P((SCR *, mtype_t, char *, size_t));
	int rval;

	CODE:
	INITMESSAGE(screen);
	rval = api_lline(screen, &last);
	ENDMESSAGE(screen);
	RETVAL=last;

	OUTPUT:
	RETVAL

void
STORESIZE(screen, count)
	VI::LINE screen
	int count

	PREINIT:
	db_recno_t last;
	void (*scr_msg) __P((SCR *, mtype_t, char *, size_t));
	int rval;

	CODE:
	INITMESSAGE(screen);
	rval = api_lline(screen, &last);
	if (!rval) {
	    if (count > last)
		rval = api_extend(screen, count);
	    else while(last && last > count) {
		rval = api_dline(screen, last--);
		if (rval) break;
	    }
	}
	ENDMESSAGE(screen);

void
EXTEND(screen, count)
	VI::LINE screen
	int count

	CODE:

void
CLEAR(screen)
	VI::LINE screen

	PREINIT:
	db_recno_t last;
	void (*scr_msg) __P((SCR *, mtype_t, char *, size_t));
	int rval;

	CODE:
	INITMESSAGE(screen);
	rval = api_lline(screen, &last);
	if (!rval) {
	    while(last) {
		rval = api_dline(screen, last--);
		if (rval) break;
	    }
	}
	ENDMESSAGE(screen);

void
PUSH(screen, ...)
	VI::LINE screen;

	PREINIT:
	db_recno_t last;
	void (*scr_msg) __P((SCR *, mtype_t, char *, size_t));
	int rval, i, len;
	char *line;

	CODE:
	INITMESSAGE(screen);
	rval = api_lline(screen, &last);

	if (!rval)
		for (i = 1; i < items; ++i) {
			line = SvPV(ST(i), len);
			if ((rval = api_aline(screen, last++, line, len)))
				break;
		}
	ENDMESSAGE(screen);

SV *
POP(screen)
	VI::LINE screen;

	PREINIT:
	db_recno_t last;
	void (*scr_msg) __P((SCR *, mtype_t, char *, size_t));
	int rval, i, len;
	CHAR_T *line;

	PPCODE:
	INITMESSAGE(screen);
	rval = api_lline(screen, &last);
	if (rval || last < 1)
		ST(0) = &PL_sv_undef;
	else {
		rval = api_gline(screen, last, &line, &len) ||
	 	       api_dline(screen, last);
		EXTEND(sp,1);
		PUSHs(sv_2mortal(newSVpv(len ? (char *)line : "", len)));
	}
	ENDMESSAGE(screen);

SV *
SHIFT(screen)
	VI::LINE screen;

	PREINIT:
	db_recno_t last;
	void (*scr_msg) __P((SCR *, mtype_t, char *, size_t));
	int rval, i, len;
	CHAR_T *line;

	PPCODE:
	INITMESSAGE(screen);
	rval = api_lline(screen, &last);
	if (rval || last < 1)
		ST(0) = &PL_sv_undef;
	else {
		rval = api_gline(screen, (db_recno_t)1, &line, &len) ||
	 	       api_dline(screen, (db_recno_t)1);
		EXTEND(sp,1);
		PUSHs(sv_2mortal(newSVpv(len ? (char *)line : "", len)));
	}
	ENDMESSAGE(screen);

void
UNSHIFT(screen, ...)
	VI::LINE screen;

	PREINIT:
	void (*scr_msg) __P((SCR *, mtype_t, char *, size_t));
	int rval, i, len;
	char *np;
	size_t nlen;
	CHAR_T *line;

	CODE:
	INITMESSAGE(screen);
	while (--items != 0) {
		np = SvPV(ST(items), nlen);
		CHAR2INTP(screen, np, nlen, line, len);
		if ((rval = api_iline(screen, (db_recno_t)1, line, len)))
			break;
	}
	ENDMESSAGE(screen);

void
SPLICE(screen, ...)
	VI::LINE screen;

	PREINIT:
	db_recno_t last, db_offset;
	void (*scr_msg) __P((SCR *, mtype_t, char *, size_t));
	int rval, length, common, len, i, offset;
	CHAR_T *line;
	char *np;
	size_t nlen;

	PPCODE:
	INITMESSAGE(screen);
	rval = api_lline(screen, &last);
	offset = items > 1 ? (int)SvIV(ST(1)) : 0;
	if (offset < 0) offset += last;
	if (offset < 0) {
	    ENDMESSAGE(screen);
	    croak("Invalid offset");
	}
	length = items > 2 ? (int)SvIV(ST(2)) : last - offset;
	if (length > last - offset)
		length = last - offset;
	db_offset = offset + 1; /* 1 based */
	EXTEND(sp,length);
	for (common = MIN(length, items - 3), i = 3; common > 0; 
	    --common, ++db_offset, --length, ++i) {
		rval |= api_gline(screen, db_offset, &line, &len);
		INT2CHAR(screen, line, len, np, nlen);
		PUSHs(sv_2mortal(newSVpv(nlen ? np : "", nlen)));
		np = SvPV(ST(i), nlen);
		CHAR2INTP(screen, np, nlen, line, len);
		rval |= api_sline(screen, db_offset, line, len);
	}
	for (; length; --length) {
		rval |= api_gline(screen, db_offset, &line, &len);
		INT2CHAR(screen, line, len, np, nlen);
		PUSHs(sv_2mortal(newSVpv(len ? np : "", nlen)));
		rval |= api_dline(screen, db_offset);
	}
	for (; i < items; ++i) {
		np = SvPV(ST(i), len);
		CHAR2INTP(screen, np, len, line, nlen);
		rval |= api_iline(screen, db_offset, line, nlen);
	}
	ENDMESSAGE(screen);

MODULE = VI	PACKAGE = VI::TAGQ

void
Add(tagq, filename, search, msg)
	VI::TAGQ    tagq;
	char	   *filename;
	char	   *search;
	char	   *msg;

	PREINIT:
	SCR *sp;

	CODE:
	sp = (SCR *)SvIV((SV*)SvRV(tagq->sprv));
	if (!sp)
		croak("screen no longer exists");
	api_tagq_add(sp, tagq->tqp, filename, search, msg);

void
Push(tagq)
	VI::TAGQ    tagq;

	PREINIT:
	SCR *sp;

	CODE:
	sp = (SCR *)SvIV((SV*)SvRV(tagq->sprv));
	if (!sp)
		croak("screen no longer exists");
	api_tagq_push(sp, &tagq->tqp);

void
DESTROY(tagq)
	# Can already be invalidated by push 
	VI::TAGQ2    tagq; 

	PREINIT:
	SCR *sp;

	CODE:
	sp = (SCR *)SvIV((SV*)SvRV(tagq->sprv));
	if (sp)
		api_tagq_free(sp, tagq->tqp);
	SvREFCNT_dec(tagq->sprv);
	free(tagq);
