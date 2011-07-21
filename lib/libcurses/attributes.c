/*	$NetBSD: attributes.c,v 1.21 2010/12/25 10:08:20 blymn Exp $	*/

/*-
 * Copyright (c) 1999 The NetBSD Foundation, Inc.
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
__RCSID("$NetBSD: attributes.c,v 1.21 2010/12/25 10:08:20 blymn Exp $");
#endif				/* not lint */

#include "curses.h"
#include "curses_private.h"

void __wcolor_set(WINDOW *, attr_t);

#ifndef _CURSES_USE_MACROS
/*
 * attr_get --
 *	Get wide attributes and color pair from stdscr
 */
/* ARGSUSED */
int
attr_get(attr_t *attr, short *pair, void *opt)
{
	return wattr_get(stdscr, attr, pair, opt);
}

/*
 * attr_on --
 *	Test and set wide attributes on stdscr
 */
/* ARGSUSED */
int
attr_on(attr_t attr, void *opt)
{
	return wattr_on(stdscr, attr, opt);
}

/*
 * attr_off --
 *	Test and unset wide attributes on stdscr
 */
/* ARGSUSED */
int
attr_off(attr_t attr, void *opt)
{
	return wattr_off(stdscr, attr, opt);
}

/*
 * attr_set --
 *	Set wide attributes and color pair on stdscr
 */
/* ARGSUSED */
int
attr_set(attr_t attr, short pair, void *opt)
{
	return wattr_set(stdscr, attr, pair, opt);
}

/*
 * color_set --
 *	Set color pair on stdscr
 */
/* ARGSUSED */
int
color_set(short pair, void *opt)
{
	return wcolor_set(stdscr, pair, opt);
}

/*
 * attron --
 *	Test and set attributes on stdscr
 */
int
attron(int attr)
{
	return wattr_on(stdscr, (attr_t) attr, NULL);
}

/*
 * attroff --
 *	Test and unset attributes on stdscr.
 */
int
attroff(int attr)
{
	return wattr_off(stdscr, (attr_t) attr, NULL);
}

/*
 * attrset --
 *	Set specific attribute modes.
 *	Unset others.  On stdscr.
 */
int
attrset(int attr)
{
	return wattrset(stdscr, attr);
}
#endif	/* _CURSES_USE_MACROS */

/*
 * wattr_get --
 *	Get wide attributes and colour pair from window
 *	Note that attributes also includes colour.
 */
/* ARGSUSED */
int
wattr_get(WINDOW *win, attr_t *attr, short *pair, void *opt)
{
#ifdef DEBUG
	__CTRACE(__CTRACE_ATTR, "wattr_get: win %p\n", win);
#endif
	if (attr != NULL) {
		*attr = win->wattr;
#ifdef HAVE_WCHAR
		*attr &= WA_ATTRIBUTES;
#endif
	}

	if (pair != NULL)
		*pair = PAIR_NUMBER(win->wattr);
	return OK;
}

/*
 * wattr_on --
 *	Test and set wide attributes on window
 */
/* ARGSUSED */
int
wattr_on(WINDOW *win, attr_t attr, void *opt)
{
#ifdef DEBUG
	__CTRACE(__CTRACE_ATTR, "wattr_on: win %p, attr %08x\n", win, attr);
#endif
	/* If can enter modes, set the relevent attribute bits. */
	if (exit_attribute_mode != NULL) {
		if (attr & __BLINK && enter_blink_mode != NULL)
			win->wattr |= __BLINK;
		if (attr & __BOLD && enter_bold_mode != NULL)
			win->wattr |= __BOLD;
		if (attr & __DIM && enter_dim_mode != NULL)
			win->wattr |= __DIM;
		if (attr & __BLANK && enter_secure_mode != NULL)
			win->wattr |= __BLANK;
		if (attr & __PROTECT && enter_protected_mode != NULL)
			win->wattr |= __PROTECT;
		if (attr & __REVERSE && enter_reverse_mode != NULL)
			win->wattr |= __REVERSE;
#ifdef HAVE_WCHAR
		if (attr & WA_LOW && enter_low_hl_mode != NULL)
			win->wattr |= WA_LOW;
		if (attr & WA_TOP && enter_top_hl_mode != NULL)
			win->wattr |= WA_TOP;
		if (attr & WA_LEFT && enter_left_hl_mode != NULL)
			win->wattr |= WA_LEFT;
		if (attr & WA_RIGHT && enter_right_hl_mode != NULL)
			win->wattr |= WA_RIGHT;
		if (attr & WA_HORIZONTAL && enter_horizontal_hl_mode != NULL)
			win->wattr |= WA_HORIZONTAL;
		if (attr & WA_VERTICAL && enter_vertical_hl_mode != NULL)
			win->wattr |= WA_VERTICAL;
#endif /* HAVE_WCHAR */
	}
	if (attr & __STANDOUT && enter_standout_mode != NULL && exit_standout_mode != NULL)
		wstandout(win);
	if (attr & __UNDERSCORE && enter_underline_mode != NULL && exit_underline_mode != NULL)
		wunderscore(win);
	if ((attr_t) attr & __COLOR)
		__wcolor_set(win, (attr_t) attr);
	return OK;
}

/*
 * wattr_off --
 *	Test and unset wide attributes on window
 *
 *	Note that the 'me' sequence unsets all attributes.  We handle
 *	which attributes should really be set in refresh.c:makech().
 */
/* ARGSUSED */
int
wattr_off(WINDOW *win, attr_t attr, void *opt)
{
#ifdef DEBUG
	__CTRACE(__CTRACE_ATTR, "wattr_off: win %p, attr %08x\n", win, attr);
#endif
	/* If can do exit modes, unset the relevent attribute bits. */
	if (exit_attribute_mode != NULL) {
		if (attr & __BLINK)
			win->wattr &= ~__BLINK;
		if (attr & __BOLD)
			win->wattr &= ~__BOLD;
		if (attr & __DIM)
			win->wattr &= ~__DIM;
		if (attr & __BLANK)
			win->wattr &= ~__BLANK;
		if (attr & __PROTECT)
			win->wattr &= ~__PROTECT;
		if (attr & __REVERSE)
			win->wattr &= ~__REVERSE;
#ifdef HAVE_WCHAR
		if (attr & WA_LOW)
			win->wattr &= ~WA_LOW;
		if (attr & WA_TOP)
			win->wattr &= ~WA_TOP;
		if (attr & WA_LEFT)
			win->wattr &= ~WA_LEFT;
		if (attr & WA_RIGHT)
			win->wattr &= ~WA_RIGHT;
		if (attr & WA_HORIZONTAL)
			win->wattr &= ~WA_HORIZONTAL;
	if (attr & WA_VERTICAL)
			win->wattr &= ~WA_VERTICAL;
#endif /* HAVE_WCHAR */
	}
	if (attr & __STANDOUT)
		wstandend(win);
	if (attr & __UNDERSCORE)
		wunderend(win);
	if ((attr_t) attr & __COLOR) {
		if (max_colors != 0)
			win->wattr &= ~__COLOR;
	}
	return OK;
}

/*
 * wattr_set --
 *	Set wide attributes and color pair on window
 */
int
wattr_set(WINDOW *win, attr_t attr, short pair, void *opt)
{
#ifdef DEBUG
	__CTRACE(__CTRACE_ATTR, "wattr_set: win %p, attr %08x, pair %d\n",
	    win, attr, pair);
#endif
 	wattr_off(win, __ATTRIBUTES, opt);
	/*
	 * This overwrites any colour setting from the attributes
	 * and is compatible with ncurses.
	 */
 	attr = (attr & ~__COLOR) | COLOR_PAIR(pair);
 	wattr_on(win, attr, opt);
	return OK;
}

/*
 * wattron --
 *	Test and set attributes.
 */
int
wattron(WINDOW *win, int attr)
{
#ifdef DEBUG
	__CTRACE(__CTRACE_ATTR, "wattron: win %p, attr %08x\n", win, attr);
#endif
	return wattr_on(win, (attr_t) attr, NULL);
}

/*
 * wattroff --
 *	Test and unset attributes.
 */
int
wattroff(WINDOW *win, int attr)
{
#ifdef DEBUG
	__CTRACE(__CTRACE_ATTR, "wattroff: win %p, attr %08x\n", win, attr);
#endif
	return wattr_off(win, (attr_t) attr, NULL);
}

/*
 * wattrset --
 *	Set specific attribute modes.
 *	Unset others.
 */
int
wattrset(WINDOW *win, int attr)
{
#ifdef DEBUG
	__CTRACE(__CTRACE_ATTR, "wattrset: win %p, attr %08x\n", win, attr);
#endif
	wattr_off(win, __ATTRIBUTES, NULL);
	wattr_on(win, (attr_t) attr, NULL);
	return OK;
}

/*
 * wcolor_set --
 *	Set color pair on window
 */
/* ARGSUSED */
int
wcolor_set(WINDOW *win, short pair, void *opt)
{
#ifdef DEBUG
	__CTRACE(__CTRACE_COLOR, "wolor_set: win %p, pair %d\n", win, pair);
#endif
	__wcolor_set(win, (attr_t) COLOR_PAIR(pair));
	return OK;
}

/*
 * getattrs --
 *	Get window attributes.
 */
chtype
getattrs(WINDOW *win)
{
#ifdef DEBUG
	__CTRACE(__CTRACE_ATTR, "getattrs: win %p\n", win);
#endif
	return((chtype) win->wattr);
}

/*
 * termattrs --
 *	Get terminal attributes
 */
chtype
termattrs(void)
{
	chtype ch = 0;

#ifdef DEBUG
	__CTRACE(__CTRACE_ATTR, "termattrs\n");
#endif
	if (exit_attribute_mode != NULL) {
#ifdef DEBUG
	__CTRACE(__CTRACE_ATTR, "termattrs: have exit attribute mode\n");
#endif
		if (enter_blink_mode != NULL)
			ch |= __BLINK;
		if (enter_bold_mode != NULL)
			ch |= __BOLD;
		if (enter_dim_mode != NULL)
			ch |= __DIM;
		if (enter_secure_mode != NULL)
			ch |= __BLANK;
		if (enter_protected_mode != NULL)
			ch |= __PROTECT;
		if (enter_reverse_mode != NULL)
			ch |= __REVERSE;
	}
	if (enter_standout_mode != NULL && exit_standout_mode != NULL)
		ch |= __STANDOUT;
	if (enter_underline_mode != NULL && exit_underline_mode != NULL)
		ch |= __UNDERSCORE;
	if (enter_alt_charset_mode != NULL && exit_alt_charset_mode != NULL)
		ch |= __ALTCHARSET;

	return ch;
}

/*
 * term_attrs --
 *	Get terminal wide attributes
 */
attr_t
term_attrs(void)
{
	attr_t attr = 0;

#ifdef DEBUG
	__CTRACE(__CTRACE_ATTR, "term_attrs\n");
#endif
	if (exit_attribute_mode != NULL) {
		if (enter_blink_mode != NULL)
			attr |= __BLINK;
		if (enter_bold_mode != NULL)
			attr |= __BOLD;
		if (enter_dim_mode != NULL)
			attr |= __DIM;
		if (enter_secure_mode != NULL)
			attr |= __BLANK;
		if (enter_protected_mode != NULL)
			attr |= __PROTECT;
		if (enter_reverse_mode != NULL)
			attr |= __REVERSE;
#ifdef HAVE_WCHAR
		if (enter_low_hl_mode != NULL)
			attr |= WA_LOW;
		if (enter_top_hl_mode != NULL)
			attr |= WA_TOP;
		if (enter_left_hl_mode != NULL)
			attr |= WA_LEFT;
		if (enter_right_hl_mode != NULL)
			attr |= WA_RIGHT;
		if (enter_horizontal_hl_mode != NULL)
			attr |= WA_HORIZONTAL;
		if (enter_vertical_hl_mode != NULL)
			attr |= WA_VERTICAL;
#endif /* HAVE_WCHAR */
	}
	if (enter_standout_mode != NULL && exit_standout_mode != NULL)
		attr |= __STANDOUT;
	if (enter_underline_mode != NULL && exit_underline_mode != NULL)
		attr |= __UNDERSCORE;
	if (enter_alt_charset_mode != NULL && exit_alt_charset_mode != NULL)
		attr |= __ALTCHARSET;

	return attr;
}

/*
 * __wcolor_set --
 * Set color attribute on window
 */
void
__wcolor_set(WINDOW *win, attr_t attr)
{
	/* If another color pair is set, turn that off first. */
	win->wattr &= ~__COLOR;
	/* If can do color video, set the color pair bits. */
	if (max_colors != 0 && attr & __COLOR)
		win->wattr |= attr & __COLOR;
}
