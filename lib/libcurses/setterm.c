/*	$NetBSD: setterm.c,v 1.47 2010/02/11 11:45:47 roy Exp $	*/

/*
 * Copyright (c) 1981, 1993, 1994
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
 */

#include <sys/cdefs.h>
#ifndef lint
#if 0
static char sccsid[] = "@(#)setterm.c	8.8 (Berkeley) 10/25/94";
#else
__RCSID("$NetBSD: setterm.c,v 1.47 2010/02/11 11:45:47 roy Exp $");
#endif
#endif /* not lint */

#include <sys/ioctl.h>		/* TIOCGWINSZ on old systems. */

#include <stdlib.h>
#include <string.h>
#include <termios.h>
#include <unistd.h>

#include "curses.h"
#include "curses_private.h"

static int does_esc_m(const char *cap);
static int does_ctrl_o(const char *cap);

attr_t	 __mask_op, __mask_me, __mask_ue, __mask_se;

int
setterm(char *type)
{
	return _cursesi_setterm(type, _cursesi_screen);
}

int
_cursesi_setterm(char *type, SCREEN *screen)
{
	int unknown, r;
	struct winsize win;
	char *p;

	if (type[0] == '\0')
		type = "xx";
	unknown = 0;
	(void)ti_setupterm(&screen->term, type, fileno(screen->outfd), &r);
	if (screen->term == NULL) {
		unknown++;
		(void)ti_setupterm(&screen->term, "dumb",
		    fileno(screen->outfd), &r);
		/* No dumb term? We can't continue */
		if (screen->term == NULL)
			return ERR;
	}
#ifdef DEBUG
	__CTRACE(__CTRACE_INIT, "setterm: tty = %s\n", type);
#endif

	/* Try TIOCGWINSZ, and, if it fails, the termcap entry. */
	if (ioctl(fileno(screen->outfd), TIOCGWINSZ, &win) != -1 &&
	    win.ws_row != 0 && win.ws_col != 0) {
		screen->LINES = win.ws_row;
		screen->COLS = win.ws_col;
	}  else {
		if (unknown) {
			screen->LINES = -1;
			screen->COLS = -1;
		} else {
			screen->LINES = t_lines(screen->term);
			screen->COLS = t_columns(screen->term);
		}
		
	}

	/* POSIX 1003.2 requires that the environment override. */
	if ((p = getenv("LINES")) != NULL)
		screen->LINES = (int) strtol(p, NULL, 0);
	if ((p = getenv("COLUMNS")) != NULL)
		screen->COLS = (int) strtol(p, NULL, 0);
	if ((p = getenv("ESCDELAY")) != NULL)
		ESCDELAY = (int) strtol(p, NULL, 0);

	/*
	 * Want cols > 4, otherwise things will fail.
	 */
	if (screen->COLS <= 4)
		return (ERR);

	LINES = screen->LINES;
	COLS = screen->COLS;

#ifdef DEBUG
	__CTRACE(__CTRACE_INIT, "setterm: LINES = %d, COLS = %d\n",
	    LINES, COLS);
#endif

	/*
	 * set the pad char, only take the first char of the pc capability
	 * as this is all we can use.
	 */
	screen->padchar = t_pad_char(screen->term) ?
	    t_pad_char(screen->term)[0] : 0; 

	/* Get full name of terminal */
	if (unknown) {
		strcpy(screen->ttytype, "dumb");
		return ERR;
	}
	if (screen->term->desc == NULL)
		screen->ttytype[0] = '\0';
	else
		strlcpy(screen->ttytype, screen->term->desc,
		    sizeof(screen->ttytype));

	/* If no scrolling commands, no quick change. */
	screen->noqch =
  	    (t_change_scroll_region(screen->term) == NULL ||
		t_cursor_home(screen->term) == NULL ||
		(t_parm_index(screen->term) == NULL &&
		    t_scroll_forward(screen->term) == NULL) ||
		(t_parm_rindex(screen->term) == NULL &&
		    t_scroll_reverse(screen->term) == NULL)) &&
	    ((t_parm_insert_line(screen->term) == NULL &&
		t_insert_line(screen->term) == NULL) ||
		(t_parm_delete_line(screen->term) == NULL &&
		    t_delete_line(screen->term) == NULL));

	/*
	 * Precalculate conflict info for color/attribute end commands.
	 */
#ifndef HAVE_WCHAR
	screen->mask_op = __ATTRIBUTES & ~__COLOR;
#else
	screen->mask_op = WA_ATTRIBUTES & ~__COLOR;
#endif /* HAVE_WCHAR */
	if (t_orig_pair(screen->term) != NULL) {
		if (does_esc_m(t_orig_pair(screen->term)))
			screen->mask_op &=
			    ~(__STANDOUT | __UNDERSCORE | __TERMATTR);
		else {
			if (t_exit_standout_mode(screen->term) != NULL &&
			    !strcmp(t_orig_pair(screen->term),
				t_exit_standout_mode(screen->term)))
				screen->mask_op &= ~__STANDOUT;
			if (t_exit_underline_mode(screen->term) != NULL &&
			    !strcmp(t_orig_pair(screen->term),
				t_exit_underline_mode(screen->term)))
				screen->mask_op &= ~__UNDERSCORE;
			if (t_exit_attribute_mode(screen->term) != NULL &&
			    !strcmp(t_orig_pair(screen->term),
				t_exit_attribute_mode(screen->term)))
				screen->mask_op &= ~__TERMATTR;
		}
	}
	/*
	 * Assume that "me" turns off all attributes apart from ACS.
	 * It might turn off ACS, so check for that.
	 */
	if (t_exit_attribute_mode(screen->term) != NULL &&
	    does_ctrl_o(t_exit_attribute_mode(screen->term)))
		screen->mask_me = 0;
	else
		screen->mask_me = __ALTCHARSET;

	/* Check what turning off the attributes also turns off */
#ifndef HAVE_WCHAR
	screen->mask_ue = __ATTRIBUTES & ~__UNDERSCORE;
#else
	screen->mask_ue = WA_ATTRIBUTES & ~__UNDERSCORE;
#endif /* HAVE_WCHAR */
	if (t_exit_underline_mode(screen->term) != NULL) {
		if (does_esc_m(t_exit_underline_mode(screen->term)))
			screen->mask_ue &=
			    ~(__STANDOUT | __TERMATTR | __COLOR);
		else {
			if (t_exit_standout_mode(screen->term) != NULL &&
			    !strcmp(t_exit_underline_mode(screen->term),
				t_exit_standout_mode(screen->term)))
				screen->mask_ue &= ~__STANDOUT;
			if (t_exit_attribute_mode(screen->term) != NULL &&
			    !strcmp(t_exit_underline_mode(screen->term),
				t_exit_attribute_mode(screen->term)))
				screen->mask_ue &= ~__TERMATTR;
			if (t_orig_pair(screen->term) != NULL &&
			    !strcmp(t_exit_underline_mode(screen->term),
				t_orig_pair(screen->term)))
				screen->mask_ue &= ~__COLOR;
		}
	}
#ifndef HAVE_WCHAR
	screen->mask_se = __ATTRIBUTES & ~__STANDOUT;
#else
	screen->mask_se = WA_ATTRIBUTES & ~__STANDOUT;
#endif /* HAVE_WCHAR */
	if (t_exit_standout_mode(screen->term) != NULL) {
		if (does_esc_m(t_exit_standout_mode(screen->term)))
			screen->mask_se &=
			    ~(__UNDERSCORE | __TERMATTR | __COLOR);
		else {
			if (t_exit_underline_mode(screen->term) != NULL &&
			    !strcmp(t_exit_standout_mode(screen->term),
				t_exit_underline_mode(screen->term)))
				screen->mask_se &= ~__UNDERSCORE;
			if (t_exit_attribute_mode(screen->term) != NULL &&
			    !strcmp(t_exit_standout_mode(screen->term),
				t_exit_attribute_mode(screen->term)))
				screen->mask_se &= ~__TERMATTR;
			if (t_orig_pair(screen->term) != NULL &&
			    !strcmp(t_exit_standout_mode(screen->term),
				t_orig_pair(screen->term)))
				screen->mask_se &= ~__COLOR;
		}
	}

	return (unknown ? ERR : OK);
}

/*
 * _cursesi_resetterm --
 *  Copy the terminal instance data for the given screen to the global
 *  variables.
 */
void
_cursesi_resetterm(SCREEN *screen)
{

	LINES = screen->LINES;
	COLS = screen->COLS;
	__GT = screen->GT;

	__noqch = screen->noqch;
	__mask_op = screen->mask_op;
	__mask_me = screen->mask_me;
	__mask_ue = screen->mask_ue;
	__mask_se = screen->mask_se;

	set_curterm(screen->term);
}

/*
 * does_esc_m --
 * A hack for xterm-like terminals where "\E[m" or "\E[0m" will turn off
 * other attributes, so we check for this in the capability passed to us.
 * Note that we can't just do simple string comparison, as the capability
 * may contain multiple, ';' separated sequence parts.
 */
static int
does_esc_m(const char *cap)
{
#define WAITING 0
#define PARSING 1
#define NUMBER 2
#define FOUND 4
	const char *capptr;
	int seq;

#ifdef DEBUG
	__CTRACE(__CTRACE_INIT, "does_esc_m: Checking %s\n", cap);
#endif
	/* Is it just "\E[m" or "\E[0m"? */
	if (!strcmp(cap, "\x1b[m") || !strcmp(cap, "\x1b[0m"))
		return 1;

	/* Does it contain a "\E[...m" sequence? */
	if (strlen(cap) < 4)
		return 0;
	capptr = cap;
	seq = WAITING;
	while (*capptr != 0) {
		switch (seq) {
		/* Start of sequence */
		case WAITING:
			if (!strncmp(capptr, "\x1b[", 2)) {
				capptr+=2;
				if (*capptr == 'm')
					return 1;
				else {
					seq=PARSING;
					continue;
				}
			}
			break;
		/* Looking for '0' */
		case PARSING:
			if (*capptr == '0')
				seq=FOUND;
			else if (*capptr > '0' && *capptr <= '9')
				seq=NUMBER;
			else if (*capptr != ';')
				seq=WAITING;
			break;
		/* Some other number */
		case NUMBER:
			if (*capptr == ';')
				seq=PARSING;
			else if (!(*capptr >= '0' && *capptr <= '9'))
				seq=WAITING;
			break;
		/* Found a '0' */
		case FOUND:
			if (*capptr == 'm')
				return 1;
			else if (!((*capptr >= '0' && *capptr <= '9') ||
			    *capptr == ';'))
				seq=WAITING;
			break;
		default:
			break;
		}
		capptr++;
	}
	return 0;
}

/*
 * does_ctrl_o --
 * A hack for vt100/xterm-like terminals where the "me" capability also
 * unsets acs (i.e. it contains the character '\017').
 */
static int
does_ctrl_o(const char *cap)
{
	const char *capptr = cap;

#ifdef DEBUG
	__CTRACE(__CTRACE_INIT, "does_ctrl_o: Looping on %s\n", capptr);
#endif
	while (*capptr != 0) {
		if (*capptr == '\x0f')
			return 1;
		capptr++;
	}
	return 0;
}
