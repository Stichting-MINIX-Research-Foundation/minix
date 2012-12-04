/*	$NetBSD: color.c,v 1.38 2011/10/03 12:32:15 roy Exp $	*/

/*
 * Copyright (c) 2000 The NetBSD Foundation, Inc.
 * All rights reserved.
 *
 * This code is derived from software contributed to The NetBSD Foundation
 * by Julian Coleman.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY THE NETBSD FOUNDATION, INC. AND CONTRIBUTORS
 * ``AS IS'' AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED
 * TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL THE FOUNDATION OR CONTRIBUTORS
 * BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 * CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
 * POSSIBILITY OF SUCH DAMAGE.
 */

#include <sys/cdefs.h>
#ifndef lint
__RCSID("$NetBSD: color.c,v 1.38 2011/10/03 12:32:15 roy Exp $");
#endif				/* not lint */

#include "curses.h"
#include "curses_private.h"

/* Have we initialised colours? */
int	__using_color = 0;

/* Default colour number */
attr_t	__default_color = 0;

/* Default colour pair values - white on black. */
struct __pair	__default_pair = {COLOR_WHITE, COLOR_BLACK, 0};

/* Default colour values */
/* Flags for colours and pairs */
#define	__USED		0x01

static void
__change_pair(short);

/*
 * has_colors --
 *	Check if terminal has colours.
 */
bool
has_colors(void)
{
	if (max_colors > 0 && max_pairs > 0 &&
	    ((set_a_foreground != NULL && set_a_background != NULL) ||
		initialize_pair != NULL || initialize_color != NULL ||
		(set_background != NULL && set_foreground != NULL)))
		return(TRUE);
	else
		return(FALSE);
}

/*
 * can_change_color --
 *	Check if terminal can change colours.
 */
bool
can_change_color(void)
{
	if (can_change)
		return(TRUE);
	else
		return(FALSE);
}

/*
 * start_color --
 *	Initialise colour support.
 */
int
start_color(void)
{
	int			 i;
	attr_t			 temp_nc;
	struct __winlist	*wlp;
	WINDOW			*win;
	int			 y, x;

	if (has_colors() == FALSE)
		return(ERR);

	/* Max colours and colour pairs */
	if (max_colors == -1)
		COLORS = 0;
	else {
		COLORS = max_colors > MAX_COLORS ? MAX_COLORS : max_colors;
		if (max_pairs == -1) {
			COLOR_PAIRS = 0;
			COLORS = 0;
		} else {
			COLOR_PAIRS = (max_pairs > MAX_PAIRS - 1 ?
			    MAX_PAIRS - 1 : max_pairs);
			 /* Use the last colour pair for curses default. */
			__default_color = COLOR_PAIR(MAX_PAIRS - 1);
		}
	}
	if (!COLORS)
		return (ERR);

	_cursesi_screen->COLORS = COLORS;
	_cursesi_screen->COLOR_PAIRS = COLOR_PAIRS;

	/* Reset terminal colour and colour pairs. */
	if (orig_colors != NULL)
		tputs(orig_colors, 0, __cputchar);
	if (orig_pair != NULL) {
		tputs(orig_pair, 0, __cputchar);
		curscr->wattr &= _cursesi_screen->mask_op;
	}

	/* Type of colour manipulation - ANSI/TEK/HP/other */
	if (set_a_foreground != NULL && set_a_background != NULL)
		_cursesi_screen->color_type = COLOR_ANSI;
	else if (initialize_pair != NULL)
		_cursesi_screen->color_type = COLOR_HP;
	else if (initialize_color != NULL)
		_cursesi_screen->color_type = COLOR_TEK;
	else if (set_foreground != NULL && set_background != NULL)
		_cursesi_screen->color_type = COLOR_OTHER;
	else
		return(ERR);		/* Unsupported colour method */

#ifdef DEBUG
	__CTRACE(__CTRACE_COLOR, "start_color: COLORS = %d, COLOR_PAIRS = %d",
	    COLORS, COLOR_PAIRS);
	switch (_cursesi_screen->color_type) {
	case COLOR_ANSI:
		__CTRACE(__CTRACE_COLOR, " (ANSI style)\n");
		break;
	case COLOR_HP:
		__CTRACE(__CTRACE_COLOR, " (HP style)\n");
		break;
	case COLOR_TEK:
		__CTRACE(__CTRACE_COLOR, " (Tektronics style)\n");
		break;
	case COLOR_OTHER:
		__CTRACE(__CTRACE_COLOR, " (Other style)\n");
		break;
	}
#endif

	/*
	 * Attributes that cannot be used with color.
	 * Store these in an attr_t for wattrset()/wattron().
	 */
	_cursesi_screen->nca = __NORMAL;
	if (no_color_video != -1) {
		temp_nc = (attr_t) t_no_color_video(_cursesi_screen->term);
		if (temp_nc & 0x0001)
			_cursesi_screen->nca |= __STANDOUT;
		if (temp_nc & 0x0002)
			_cursesi_screen->nca |= __UNDERSCORE;
		if (temp_nc & 0x0004)
			_cursesi_screen->nca |= __REVERSE;
		if (temp_nc & 0x0008)
			_cursesi_screen->nca |= __BLINK;
		if (temp_nc & 0x0010)
			_cursesi_screen->nca |= __DIM;
		if (temp_nc & 0x0020)
			_cursesi_screen->nca |= __BOLD;
		if (temp_nc & 0x0040)
			_cursesi_screen->nca |= __BLANK;
		if (temp_nc & 0x0080)
			_cursesi_screen->nca |= __PROTECT;
		if (temp_nc & 0x0100)
			_cursesi_screen->nca |= __ALTCHARSET;
	}
#ifdef DEBUG
	__CTRACE(__CTRACE_COLOR, "start_color: _cursesi_screen->nca = %08x\n",
	    _cursesi_screen->nca);
#endif

	/* Set up initial 8 colours */
	if (COLORS >= COLOR_BLACK)
		(void) init_color(COLOR_BLACK, 0, 0, 0);
	if (COLORS >= COLOR_RED)
		(void) init_color(COLOR_RED, 1000, 0, 0);
	if (COLORS >= COLOR_GREEN)
		(void) init_color(COLOR_GREEN, 0, 1000, 0);
	if (COLORS >= COLOR_YELLOW)
		(void) init_color(COLOR_YELLOW, 1000, 1000, 0);
	if (COLORS >= COLOR_BLUE)
		(void) init_color(COLOR_BLUE, 0, 0, 1000);
	if (COLORS >= COLOR_MAGENTA)
		(void) init_color(COLOR_MAGENTA, 1000, 0, 1000);
	if (COLORS >= COLOR_CYAN)
		(void) init_color(COLOR_CYAN, 0, 1000, 1000);
	if (COLORS >= COLOR_WHITE)
		(void) init_color(COLOR_WHITE, 1000, 1000, 1000);

	/* Initialise other colours */
	for (i = 8; i < COLORS; i++) {
		_cursesi_screen->colours[i].red = 0;
		_cursesi_screen->colours[i].green = 0;
		_cursesi_screen->colours[i].blue = 0;
		_cursesi_screen->colours[i].flags = 0;
	}

	/* Initialise pair 0 to default colours. */
	_cursesi_screen->colour_pairs[0].fore = -1;
	_cursesi_screen->colour_pairs[0].back = -1;
	_cursesi_screen->colour_pairs[0].flags = 0;

	/* Initialise user colour pairs to default (white on black) */
	for (i = 0; i < COLOR_PAIRS; i++) {
		_cursesi_screen->colour_pairs[i].fore = COLOR_WHITE;
		_cursesi_screen->colour_pairs[i].back = COLOR_BLACK;
		_cursesi_screen->colour_pairs[i].flags = 0;
	}

	/* Initialise default colour pair. */
	_cursesi_screen->colour_pairs[PAIR_NUMBER(__default_color)].fore =
	    __default_pair.fore;
	_cursesi_screen->colour_pairs[PAIR_NUMBER(__default_color)].back =
	    __default_pair.back;
	_cursesi_screen->colour_pairs[PAIR_NUMBER(__default_color)].flags =
	    __default_pair.flags;

	__using_color = 1;

	/* Set all positions on all windows to curses default colours. */
	for (wlp = _cursesi_screen->winlistp; wlp != NULL; wlp = wlp->nextp) {
		win = wlp->winp;
		if (wlp->winp != __virtscr && wlp->winp != curscr) {
			/* Set color attribute on other windows */
			win->battr |= __default_color;
			for (y = 0; y < win->maxy; y++) {
				for (x = 0; x < win->maxx; x++) {
					win->alines[y]->line[x].attr &= ~__COLOR;
					win->alines[y]->line[x].attr |= __default_color;
				}
			}
			__touchwin(win);
		}
	}

	return(OK);
}

/*
 * init_pair --
 *	Set pair foreground and background colors.
 *	Our default colour ordering is ANSI - 1 = red, 4 = blue, 3 = yellow,
 *	6 = cyan.  The older style (Sb/Sf) uses 1 = blue, 4 = red, 3 = cyan,
 *	6 = yellow, so we swap them here and in pair_content().
 */
int
init_pair(short pair, short fore, short back)
{
	int	changed;

#ifdef DEBUG
	__CTRACE(__CTRACE_COLOR, "init_pair: %d, %d, %d\n", pair, fore, back);
#endif

	if (pair < 0 || pair >= COLOR_PAIRS)
		return (ERR);

	if (pair == 0) /* Ignore request for pair 0, it is default. */
		return OK;

	if (fore >= COLORS)
		return (ERR);
	if (back >= COLORS)
		return (ERR);

	/* Swap red/blue and yellow/cyan */
	if (_cursesi_screen->color_type == COLOR_OTHER) {
		switch (fore) {
		case COLOR_RED:
			fore = COLOR_BLUE;
			break;
		case COLOR_BLUE:
			fore = COLOR_RED;
			break;
		case COLOR_YELLOW:
			fore = COLOR_CYAN;
			break;
		case COLOR_CYAN:
			fore = COLOR_YELLOW;
			break;
		}
		switch (back) {
		case COLOR_RED:
			back = COLOR_BLUE;
			break;
		case COLOR_BLUE:
			back = COLOR_RED;
			break;
		case COLOR_YELLOW:
			back = COLOR_CYAN;
			break;
		case COLOR_CYAN:
			back = COLOR_YELLOW;
			break;
		}
	}

	if ((_cursesi_screen->colour_pairs[pair].flags & __USED) &&
	    (fore != _cursesi_screen->colour_pairs[pair].fore ||
	     back != _cursesi_screen->colour_pairs[pair].back))
		changed = 1;
	else
		changed = 0;

	_cursesi_screen->colour_pairs[pair].flags |= __USED;
	_cursesi_screen->colour_pairs[pair].fore = fore;
	_cursesi_screen->colour_pairs[pair].back = back;

	/* XXX: need to initialise HP style (Ip) */

	if (changed)
		__change_pair(pair);
	return (OK);
}

/*
 * pair_content --
 *	Get pair foreground and background colours.
 */
int
pair_content(short pair, short *forep, short *backp)
{
	if (pair < 0 || pair > _cursesi_screen->COLOR_PAIRS)
		return(ERR);

	*forep = _cursesi_screen->colour_pairs[pair].fore;
	*backp = _cursesi_screen->colour_pairs[pair].back;

	/* Swap red/blue and yellow/cyan */
	if (_cursesi_screen->color_type == COLOR_OTHER) {
		switch (*forep) {
		case COLOR_RED:
			*forep = COLOR_BLUE;
			break;
		case COLOR_BLUE:
			*forep = COLOR_RED;
			break;
		case COLOR_YELLOW:
			*forep = COLOR_CYAN;
			break;
		case COLOR_CYAN:
			*forep = COLOR_YELLOW;
			break;
		}
		switch (*backp) {
		case COLOR_RED:
			*backp = COLOR_BLUE;
			break;
		case COLOR_BLUE:
			*backp = COLOR_RED;
			break;
		case COLOR_YELLOW:
			*backp = COLOR_CYAN;
			break;
		case COLOR_CYAN:
			*backp = COLOR_YELLOW;
			break;
		}
	}
	return(OK);
}

/*
 * init_color --
 *	Set colour red, green and blue values.
 */
int
init_color(short color, short red, short green, short blue)
{
#ifdef DEBUG
	__CTRACE(__CTRACE_COLOR, "init_color: %d, %d, %d, %d\n",
	    color, red, green, blue);
#endif
	if (color < 0 || color >= _cursesi_screen->COLORS)
		return(ERR);

	_cursesi_screen->colours[color].red = red;
	_cursesi_screen->colours[color].green = green;
	_cursesi_screen->colours[color].blue = blue;
	/* XXX Not yet implemented */
	return(ERR);
	/* XXX: need to initialise Tek style (Ic) and support HLS */
}

/*
 * color_content --
 *	Get colour red, green and blue values.
 */
int
color_content(short color, short *redp, short *greenp, short *bluep)
{
	if (color < 0 || color >= _cursesi_screen->COLORS)
		return(ERR);

	*redp = _cursesi_screen->colours[color].red;
	*greenp = _cursesi_screen->colours[color].green;
	*bluep = _cursesi_screen->colours[color].blue;
	return(OK);
}

/*
 * use_default_colors --
 *	Use terminal default colours instead of curses default colour.
  */
int
use_default_colors()
{
#ifdef DEBUG
	__CTRACE(__CTRACE_COLOR, "use_default_colors\n");
#endif

	return(assume_default_colors(-1, -1));
}

/*
 * assume_default_colors --
 *	Set the default foreground and background colours.
 */
int
assume_default_colors(short fore, short back)
{
#ifdef DEBUG
	__CTRACE(__CTRACE_COLOR, "assume_default_colors: %d, %d\n",
	    fore, back);
	__CTRACE(__CTRACE_COLOR, "assume_default_colors: default_colour = %d, pair_number = %d\n", __default_color, PAIR_NUMBER(__default_color));
#endif

	/* Swap red/blue and yellow/cyan */
	if (_cursesi_screen->color_type == COLOR_OTHER) {
		switch (fore) {
		case COLOR_RED:
			fore = COLOR_BLUE;
			break;
		case COLOR_BLUE:
			fore = COLOR_RED;
			break;
		case COLOR_YELLOW:
			fore = COLOR_CYAN;
			break;
		case COLOR_CYAN:
			fore = COLOR_YELLOW;
			break;
		}
		switch (back) {
		case COLOR_RED:
			back = COLOR_BLUE;
			break;
		case COLOR_BLUE:
			back = COLOR_RED;
			break;
		case COLOR_YELLOW:
			back = COLOR_CYAN;
			break;
		case COLOR_CYAN:
			back = COLOR_YELLOW;
			break;
		}
	}
	__default_pair.fore = fore;
	__default_pair.back = back;
	__default_pair.flags = __USED;

	if (COLOR_PAIRS) {
		_cursesi_screen->colour_pairs[PAIR_NUMBER(__default_color)].fore = fore;
		_cursesi_screen->colour_pairs[PAIR_NUMBER(__default_color)].back = back;
		_cursesi_screen->colour_pairs[PAIR_NUMBER(__default_color)].flags = __USED;
	}

	/*
	 * If we've already called start_color(), make sure all instances
	 * of the curses default colour pair are dirty.
	 */
	if (__using_color)
		__change_pair(PAIR_NUMBER(__default_color));

	return(OK);
}

/* no_color_video is a terminfo macro, but we need to retain binary compat */
#ifdef __strong_alias
#undef no_color_video
__strong_alias(no_color_video, no_color_attributes)
#endif
/*
 * no_color_attributes --
 *	Return attributes that cannot be combined with color.
 */
attr_t
no_color_attributes(void)
{
	return(_cursesi_screen->nca);
}

/*
 * __set_color --
 *	Set terminal foreground and background colours.
 */
void
__set_color( /*ARGSUSED*/ WINDOW *win, attr_t attr)
{
	short	pair;

	if ((curscr->wattr & __COLOR) == (attr & __COLOR))
		return;

	pair = PAIR_NUMBER((u_int32_t)attr);
#ifdef DEBUG
	__CTRACE(__CTRACE_COLOR, "__set_color: %d, %d, %d\n", pair,
		 _cursesi_screen->colour_pairs[pair].fore,
		 _cursesi_screen->colour_pairs[pair].back);
#endif
	switch (_cursesi_screen->color_type) {
	/* Set ANSI forground and background colours */
	case COLOR_ANSI:
		if (_cursesi_screen->colour_pairs[pair].fore < 0 ||
		    _cursesi_screen->colour_pairs[pair].back < 0)
			__unset_color(curscr);
		if (_cursesi_screen->colour_pairs[pair].fore >= 0)
			tputs(tiparm(t_set_a_foreground(_cursesi_screen->term),
			    (int)_cursesi_screen->colour_pairs[pair].fore),
			    0, __cputchar);
		if (_cursesi_screen->colour_pairs[pair].back >= 0)
			tputs(tiparm(t_set_a_background(_cursesi_screen->term),
			    (int)_cursesi_screen->colour_pairs[pair].back),
			    0, __cputchar);
		break;
	case COLOR_HP:
		/* XXX: need to support HP style */
		break;
	case COLOR_TEK:
		/* XXX: need to support Tek style */
		break;
	case COLOR_OTHER:
		if (_cursesi_screen->colour_pairs[pair].fore < 0 ||
		    _cursesi_screen->colour_pairs[pair].back < 0)
			__unset_color(curscr);
		if (_cursesi_screen->colour_pairs[pair].fore >= 0)
			tputs(tiparm(t_set_foreground(_cursesi_screen->term),
			    (int)_cursesi_screen->colour_pairs[pair].fore),
			    0, __cputchar);
		if (_cursesi_screen->colour_pairs[pair].back >= 0)
			tputs(tiparm(t_set_background(_cursesi_screen->term),
			    (int)_cursesi_screen->colour_pairs[pair].back),
			    0, __cputchar);
		break;
	}
	curscr->wattr &= ~__COLOR;
	curscr->wattr |= attr & __COLOR;
}

/*
 * __unset_color --
 *	Clear terminal foreground and background colours.
 */
void
__unset_color(WINDOW *win)
{
#ifdef DEBUG
	__CTRACE(__CTRACE_COLOR, "__unset_color\n");
#endif
	switch (_cursesi_screen->color_type) {
	/* Clear ANSI forground and background colours */
	case COLOR_ANSI:
		if (orig_pair != NULL) {
			tputs(orig_pair, 0, __cputchar);
			win->wattr &= __mask_op;
		}
		break;
	case COLOR_HP:
		/* XXX: need to support HP style */
		break;
	case COLOR_TEK:
		/* XXX: need to support Tek style */
		break;
	case COLOR_OTHER:
		if (orig_pair != NULL) {
			tputs(orig_pair, 0, __cputchar);
			win->wattr &= __mask_op;
		}
		break;
	}
}

/*
 * __restore_colors --
 *	Redo color definitions after restarting 'curses' mode.
 */
void
__restore_colors(void)
{
	if (can_change != 0)
		switch (_cursesi_screen->color_type) {
		case COLOR_HP:
			/* XXX: need to re-initialise HP style (Ip) */
			break;
		case COLOR_TEK:
			/* XXX: need to re-initialise Tek style (Ic) */
			break;
		}
}

/*
 * __change_pair --
 *	Mark dirty all positions using pair.
 */
void
__change_pair(short pair)
{
	struct __winlist	*wlp;
	WINDOW			*win;
	int			 y, x;
	__LINE			*lp;
	uint32_t		cl = COLOR_PAIR(pair);


	for (wlp = _cursesi_screen->winlistp; wlp != NULL; wlp = wlp->nextp) {
#ifdef DEBUG
		__CTRACE(__CTRACE_COLOR, "__change_pair: win = %p\n",
		    wlp->winp);
#endif
		win = wlp->winp;
		if (win == __virtscr)
			continue;
		else if (win == curscr) {
			/* Reset colour attribute on curscr */
#ifdef DEBUG
			__CTRACE(__CTRACE_COLOR,
			    "__change_pair: win == curscr\n");
#endif
			for (y = 0; y < curscr->maxy; y++) {
				lp = curscr->alines[y];
				for (x = 0; x < curscr->maxx; x++) {
					if ((lp->line[x].attr & __COLOR) == cl)
						lp->line[x].attr &= ~__COLOR;
				}
			}
		} else {
			/* Mark dirty those positions with colour pair "pair" */
			for (y = 0; y < win->maxy; y++) {
				lp = win->alines[y];
				for (x = 0; x < win->maxx; x++)
					if ((lp->line[x].attr &
					    __COLOR) == cl) {
						if (!(lp->flags & __ISDIRTY))
							lp->flags |= __ISDIRTY;
						/*
					 	* firstchp/lastchp are shared
					 	* between parent window and
					 	* sub-window.
					 	*/
						if (*lp->firstchp > x)
						*lp->firstchp = x;
						if (*lp->lastchp < x)
							*lp->lastchp = x;
					}
#ifdef DEBUG
				if ((win->alines[y]->flags & __ISDIRTY))
					__CTRACE(__CTRACE_COLOR,
					    "__change_pair: first = %d, "
					    "last = %d\n",
					    *win->alines[y]->firstchp,
					    *win->alines[y]->lastchp);
#endif
			}
		}
	}
}
