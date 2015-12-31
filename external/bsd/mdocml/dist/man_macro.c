/*	Id: man_macro.c,v 1.80 2013/12/31 23:23:10 schwarze Exp  */
/*
 * Copyright (c) 2008, 2009, 2010, 2011 Kristaps Dzonsons <kristaps@bsd.lv>
 * Copyright (c) 2012, 2013 Ingo Schwarze <schwarze@openbsd.org>
 * Copyright (c) 2013 Franco Fichtner <franco@lastsummer.de>
 *
 * Permission to use, copy, modify, and distribute this software for any
 * purpose with or without fee is hereby granted, provided that the above
 * copyright notice and this permission notice appear in all copies.
 *
 * THE SOFTWARE IS PROVIDED "AS IS" AND THE AUTHOR DISCLAIMS ALL WARRANTIES
 * WITH REGARD TO THIS SOFTWARE INCLUDING ALL IMPLIED WARRANTIES OF
 * MERCHANTABILITY AND FITNESS. IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR
 * ANY SPECIAL, DIRECT, INDIRECT, OR CONSEQUENTIAL DAMAGES OR ANY DAMAGES
 * WHATSOEVER RESULTING FROM LOSS OF USE, DATA OR PROFITS, WHETHER IN AN
 * ACTION OF CONTRACT, NEGLIGENCE OR OTHER TORTIOUS ACTION, ARISING OUT OF
 * OR IN CONNECTION WITH THE USE OR PERFORMANCE OF THIS SOFTWARE.
 */
#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include <assert.h>
#include <ctype.h>
#include <stdlib.h>
#include <string.h>

#include "man.h"
#include "mandoc.h"
#include "libmandoc.h"
#include "libman.h"

enum	rew {
	REW_REWIND,
	REW_NOHALT,
	REW_HALT
};

static	int		 blk_close(MACRO_PROT_ARGS);
static	int		 blk_exp(MACRO_PROT_ARGS);
static	int		 blk_imp(MACRO_PROT_ARGS);
static	int		 in_line_eoln(MACRO_PROT_ARGS);
static	int		 man_args(struct man *, int, 
				int *, char *, char **);

static	int		 rew_scope(enum man_type, 
				struct man *, enum mant);
static	enum rew	 rew_dohalt(enum mant, enum man_type, 
				const struct man_node *);
static	enum rew	 rew_block(enum mant, enum man_type, 
				const struct man_node *);
static	void		 rew_warn(struct man *, 
				struct man_node *, enum mandocerr);

const	struct man_macro __man_macros[MAN_MAX] = {
	{ in_line_eoln, MAN_NSCOPED }, /* br */
	{ in_line_eoln, MAN_BSCOPE }, /* TH */
	{ blk_imp, MAN_BSCOPE | MAN_SCOPED }, /* SH */
	{ blk_imp, MAN_BSCOPE | MAN_SCOPED }, /* SS */
	{ blk_imp, MAN_BSCOPE | MAN_SCOPED | MAN_FSCOPED }, /* TP */
	{ blk_imp, MAN_BSCOPE }, /* LP */
	{ blk_imp, MAN_BSCOPE }, /* PP */
	{ blk_imp, MAN_BSCOPE }, /* P */
	{ blk_imp, MAN_BSCOPE }, /* IP */
	{ blk_imp, MAN_BSCOPE }, /* HP */
	{ in_line_eoln, MAN_SCOPED }, /* SM */
	{ in_line_eoln, MAN_SCOPED }, /* SB */
	{ in_line_eoln, 0 }, /* BI */
	{ in_line_eoln, 0 }, /* IB */
	{ in_line_eoln, 0 }, /* BR */
	{ in_line_eoln, 0 }, /* RB */
	{ in_line_eoln, MAN_SCOPED }, /* R */
	{ in_line_eoln, MAN_SCOPED }, /* B */
	{ in_line_eoln, MAN_SCOPED }, /* I */
	{ in_line_eoln, 0 }, /* IR */
	{ in_line_eoln, 0 }, /* RI */
	{ in_line_eoln, MAN_NSCOPED }, /* na */
	{ in_line_eoln, MAN_NSCOPED }, /* sp */
	{ in_line_eoln, MAN_BSCOPE }, /* nf */
	{ in_line_eoln, MAN_BSCOPE }, /* fi */
	{ blk_close, 0 }, /* RE */
	{ blk_exp, MAN_BSCOPE | MAN_EXPLICIT }, /* RS */
	{ in_line_eoln, 0 }, /* DT */
	{ in_line_eoln, 0 }, /* UC */
	{ in_line_eoln, 0 }, /* PD */
	{ in_line_eoln, 0 }, /* AT */
	{ in_line_eoln, 0 }, /* in */
	{ in_line_eoln, 0 }, /* ft */
	{ in_line_eoln, 0 }, /* OP */
	{ in_line_eoln, MAN_BSCOPE }, /* EX */
	{ in_line_eoln, MAN_BSCOPE }, /* EE */
	{ blk_exp, MAN_BSCOPE | MAN_EXPLICIT }, /* UR */
	{ blk_close, 0 }, /* UE */
};

const	struct man_macro * const man_macros = __man_macros;


/*
 * Warn when "n" is an explicit non-roff macro.
 */
static void
rew_warn(struct man *man, struct man_node *n, enum mandocerr er)
{

	if (er == MANDOCERR_MAX || MAN_BLOCK != n->type)
		return;
	if (MAN_VALID & n->flags)
		return;
	if ( ! (MAN_EXPLICIT & man_macros[n->tok].flags))
		return;

	assert(er < MANDOCERR_FATAL);
	man_nmsg(man, n, er);
}


/*
 * Rewind scope.  If a code "er" != MANDOCERR_MAX has been provided, it
 * will be used if an explicit block scope is being closed out.
 */
int
man_unscope(struct man *man, const struct man_node *to, 
		enum mandocerr er)
{
	struct man_node	*n;

	assert(to);

	man->next = MAN_NEXT_SIBLING;

	/* LINTED */
	while (man->last != to) {
		/*
		 * Save the parent here, because we may delete the
		 * man->last node in the post-validation phase and reset
		 * it to man->last->parent, causing a step in the closing
		 * out to be lost.
		 */
		n = man->last->parent;
		rew_warn(man, man->last, er);
		if ( ! man_valid_post(man))
			return(0);
		man->last = n;
		assert(man->last);
	}

	rew_warn(man, man->last, er);
	if ( ! man_valid_post(man))
		return(0);

	return(1);
}


static enum rew
rew_block(enum mant ntok, enum man_type type, const struct man_node *n)
{

	if (MAN_BLOCK == type && ntok == n->parent->tok && 
			MAN_BODY == n->parent->type)
		return(REW_REWIND);
	return(ntok == n->tok ? REW_HALT : REW_NOHALT);
}


/*
 * There are three scope levels: scoped to the root (all), scoped to the
 * section (all less sections), and scoped to subsections (all less
 * sections and subsections).
 */
static enum rew 
rew_dohalt(enum mant tok, enum man_type type, const struct man_node *n)
{
	enum rew	 c;

	/* We cannot progress beyond the root ever. */
	if (MAN_ROOT == n->type)
		return(REW_HALT);

	assert(n->parent);

	/* Normal nodes shouldn't go to the level of the root. */
	if (MAN_ROOT == n->parent->type)
		return(REW_REWIND);

	/* Already-validated nodes should be closed out. */
	if (MAN_VALID & n->flags)
		return(REW_NOHALT);

	/* First: rewind to ourselves. */
	if (type == n->type && tok == n->tok) {
		if (MAN_EXPLICIT & man_macros[n->tok].flags)
			return(REW_HALT);
		else
			return(REW_REWIND);
	}

	/* 
	 * Next follow the implicit scope-smashings as defined by man.7:
	 * section, sub-section, etc.
	 */

	switch (tok) {
	case (MAN_SH):
		break;
	case (MAN_SS):
		/* Rewind to a section, if a block. */
		if (REW_NOHALT != (c = rew_block(MAN_SH, type, n)))
			return(c);
		break;
	case (MAN_RS):
		/* Preserve empty paragraphs before RS. */
		if (0 == n->nchild && (MAN_P == n->tok ||
		    MAN_PP == n->tok || MAN_LP == n->tok))
			return(REW_HALT);
		/* Rewind to a subsection, if a block. */
		if (REW_NOHALT != (c = rew_block(MAN_SS, type, n)))
			return(c);
		/* Rewind to a section, if a block. */
		if (REW_NOHALT != (c = rew_block(MAN_SH, type, n)))
			return(c);
		break;
	default:
		/* Rewind to an offsetter, if a block. */
		if (REW_NOHALT != (c = rew_block(MAN_RS, type, n)))
			return(c);
		/* Rewind to a subsection, if a block. */
		if (REW_NOHALT != (c = rew_block(MAN_SS, type, n)))
			return(c);
		/* Rewind to a section, if a block. */
		if (REW_NOHALT != (c = rew_block(MAN_SH, type, n)))
			return(c);
		break;
	}

	return(REW_NOHALT);
}


/*
 * Rewinding entails ascending the parse tree until a coherent point,
 * for example, the `SH' macro will close out any intervening `SS'
 * scopes.  When a scope is closed, it must be validated and actioned.
 */
static int
rew_scope(enum man_type type, struct man *man, enum mant tok)
{
	struct man_node	*n;
	enum rew	 c;

	/* LINTED */
	for (n = man->last; n; n = n->parent) {
		/* 
		 * Whether we should stop immediately (REW_HALT), stop
		 * and rewind until this point (REW_REWIND), or keep
		 * rewinding (REW_NOHALT).
		 */
		c = rew_dohalt(tok, type, n);
		if (REW_HALT == c)
			return(1);
		if (REW_REWIND == c)
			break;
	}

	/* 
	 * Rewind until the current point.  Warn if we're a roff
	 * instruction that's mowing over explicit scopes.
	 */
	assert(n);

	return(man_unscope(man, n, MANDOCERR_MAX));
}


/*
 * Close out a generic explicit macro.
 */
/* ARGSUSED */
int
blk_close(MACRO_PROT_ARGS)
{
	enum mant	 	 ntok;
	const struct man_node	*nn;

	switch (tok) {
	case (MAN_RE):
		ntok = MAN_RS;
		break;
	case (MAN_UE):
		ntok = MAN_UR;
		break;
	default:
		abort();
		/* NOTREACHED */
	}

	for (nn = man->last->parent; nn; nn = nn->parent)
		if (ntok == nn->tok && MAN_BLOCK == nn->type)
			break;

	if (NULL == nn) {
		man_pmsg(man, line, ppos, MANDOCERR_NOSCOPE);
		if ( ! rew_scope(MAN_BLOCK, man, MAN_PP))
			return(0);
	} else 
		man_unscope(man, nn, MANDOCERR_MAX);

	return(1);
}


/* ARGSUSED */
int
blk_exp(MACRO_PROT_ARGS)
{
	struct man_node	*n;
	int		 la;
	char		*p;

	/* Close out prior implicit scopes. */

	if ( ! rew_scope(MAN_BLOCK, man, tok))
		return(0);

	if ( ! man_block_alloc(man, line, ppos, tok))
		return(0);
	if ( ! man_head_alloc(man, line, ppos, tok))
		return(0);

	for (;;) {
		la = *pos;
		if ( ! man_args(man, line, pos, buf, &p))
			break;
		if ( ! man_word_alloc(man, line, la, p))
			return(0);
	}

	assert(man);
	assert(tok != MAN_MAX);

	for (n = man->last; n; n = n->parent) {
		if (n->tok != tok)
			continue;
		assert(MAN_HEAD == n->type);
		man_unscope(man, n, MANDOCERR_MAX);
		break;
	}

	return(man_body_alloc(man, line, ppos, tok));
}



/*
 * Parse an implicit-block macro.  These contain a MAN_HEAD and a
 * MAN_BODY contained within a MAN_BLOCK.  Rules for closing out other
 * scopes, such as `SH' closing out an `SS', are defined in the rew
 * routines.
 */
/* ARGSUSED */
int
blk_imp(MACRO_PROT_ARGS)
{
	int		 la;
	char		*p;
	struct man_node	*n;

	/* Close out prior scopes. */

	if ( ! rew_scope(MAN_BODY, man, tok))
		return(0);
	if ( ! rew_scope(MAN_BLOCK, man, tok))
		return(0);

	/* Allocate new block & head scope. */

	if ( ! man_block_alloc(man, line, ppos, tok))
		return(0);
	if ( ! man_head_alloc(man, line, ppos, tok))
		return(0);

	n = man->last;

	/* Add line arguments. */

	for (;;) {
		la = *pos;
		if ( ! man_args(man, line, pos, buf, &p))
			break;
		if ( ! man_word_alloc(man, line, la, p))
			return(0);
	}

	/* Close out head and open body (unless MAN_SCOPE). */

	if (MAN_SCOPED & man_macros[tok].flags) {
		/* If we're forcing scope (`TP'), keep it open. */
		if (MAN_FSCOPED & man_macros[tok].flags) {
			man->flags |= MAN_BLINE;
			return(1);
		} else if (n == man->last) {
			man->flags |= MAN_BLINE;
			return(1);
		}
	}

	if ( ! rew_scope(MAN_HEAD, man, tok))
		return(0);
	return(man_body_alloc(man, line, ppos, tok));
}


/* ARGSUSED */
int
in_line_eoln(MACRO_PROT_ARGS)
{
	int		 la;
	char		*p;
	struct man_node	*n;

	if ( ! man_elem_alloc(man, line, ppos, tok))
		return(0);

	n = man->last;

	for (;;) {
		la = *pos;
		if ( ! man_args(man, line, pos, buf, &p))
			break;
		if ( ! man_word_alloc(man, line, la, p))
			return(0);
	}

	/*
	 * Append MAN_EOS in case the last snipped argument
	 * ends with a dot, e.g. `.IR syslog (3).'
	 */

	if (n != man->last &&
	    mandoc_eos(man->last->string, strlen(man->last->string)))
		man->last->flags |= MAN_EOS;

	/*
	 * If no arguments are specified and this is MAN_SCOPED (i.e.,
	 * next-line scoped), then set our mode to indicate that we're
	 * waiting for terms to load into our context.
	 */

	if (n == man->last && MAN_SCOPED & man_macros[tok].flags) {
		assert( ! (MAN_NSCOPED & man_macros[tok].flags));
		man->flags |= MAN_ELINE;
		return(1);
	} 

	/* Set ignorable context, if applicable. */

	if (MAN_NSCOPED & man_macros[tok].flags) {
		assert( ! (MAN_SCOPED & man_macros[tok].flags));
		man->flags |= MAN_ILINE;
	}

	assert(MAN_ROOT != man->last->type);
	man->next = MAN_NEXT_SIBLING;
	
	/*
	 * Rewind our element scope.  Note that when TH is pruned, we'll
	 * be back at the root, so make sure that we don't clobber as
	 * its sibling.
	 */

	for ( ; man->last; man->last = man->last->parent) {
		if (man->last == n)
			break;
		if (man->last->type == MAN_ROOT)
			break;
		if ( ! man_valid_post(man))
			return(0);
	}

	assert(man->last);

	/*
	 * Same here regarding whether we're back at the root. 
	 */

	if (man->last->type != MAN_ROOT && ! man_valid_post(man))
		return(0);

	return(1);
}


int
man_macroend(struct man *man)
{

	return(man_unscope(man, man->first, MANDOCERR_SCOPEEXIT));
}

static int
man_args(struct man *man, int line, int *pos, char *buf, char **v)
{
	char	 *start;

	assert(*pos);
	*v = start = buf + *pos;
	assert(' ' != *start);

	if ('\0' == *start)
		return(0);

	*v = mandoc_getarg(man->parse, v, line, pos);
	return(1);
}
