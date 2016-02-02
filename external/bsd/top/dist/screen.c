/*
 * Copyright (c) 1984 through 2008, William LeFebvre
 * All rights reserved.
 * 
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are met:
 * 
 *     * Redistributions of source code must retain the above copyright
 * notice, this list of conditions and the following disclaimer.
 * 
 *     * Redistributions in binary form must reproduce the above
 * copyright notice, this list of conditions and the following disclaimer
 * in the documentation and/or other materials provided with the
 * distribution.
 * 
 *     * Neither the name of William LeFebvre nor the names of other
 * contributors may be used to endorse or promote products derived from
 * this software without specific prior written permission.
 * 
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
 * A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
 * OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 * SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
 * LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

/*
 *  Top users/processes display for Unix
 *  Version 3
 */

/*  This file contains the routines that interface to termcap and stty/gtty.
 *
 *  Paul Vixie, February 1987: converted to use ioctl() instead of stty/gtty.
 *
 *  I put in code to turn on the TOSTOP bit while top was running, but I
 *  didn't really like the results.  If you desire it, turn on the
 *  preprocessor variable "TOStop".   --wnl
 */

#include "os.h"
#include "top.h"

#if HAVE_CURSES_H && HAVE_TERM_H
#include <curses.h>
#include <term.h>
#else
#if HAVE_TERMCAP_H
#include <termcap.h>
#else
#if HAVE_CURSES_H
#include <curses.h>
#endif
#endif
#endif

#if !HAVE_DECL_TPUTS
int tputs(const char *, int, int (*)(int));
#endif
#if !HAVE_DECL_TGOTO
char *tgoto(const char *, int, int);
#endif
#if !HAVE_DECL_TGETENT
int tgetent(const char *, char *);
#endif
#if !HAVE_DECL_TGETFLAG
int tgetflag(const char *);
#endif
#if !HAVE_DECL_TGETNUM
int tgetnum(const char *);
#endif
#if !HAVE_DECL_TGETSTR
char *tgetstr(const char *, char **);
#endif

#include <sys/ioctl.h>
#ifdef CBREAK
# include <sgtty.h>
# define USE_SGTTY
#else
# ifdef TCGETA
#  define USE_TERMIO
#  include <termio.h>
# else
#  define USE_TERMIOS
#  include <termios.h>
# endif
#endif
#if defined(USE_TERMIO) || defined(USE_TERMIOS)
# ifndef TAB3
#  ifdef OXTABS
#   define TAB3 OXTABS
#  else
#   define TAB3 0
#  endif
# endif
#endif

#include "screen.h"
#include "boolean.h"

#define putcap(str)     ((str) != NULL ? (void)tputs(str, 1, putstdout) : (void)0)

extern char *myname;

char ch_erase;
char ch_kill;
char ch_werase;
char smart_terminal;
int  screen_length;
int  screen_width;

char PC;

static int  tc_overstrike;
static char termcap_buf[1024];
static char string_buffer[1024];
static char home[15];
static char lower_left[15];
static char *tc_clear_line;
static char *tc_clear_screen;
static char *tc_clear_to_end;
static char *tc_cursor_motion;
static char *tc_start_standout;
static char *tc_end_standout;
static char *terminal_init;
static char *terminal_end;

#ifdef USE_SGTTY
static struct sgttyb old_settings;
static struct sgttyb new_settings;
#endif
#ifdef USE_TERMIO
static struct termio old_settings;
static struct termio new_settings;
#endif
#ifdef USE_TERMIOS
static struct termios old_settings;
static struct termios new_settings;
#endif
static char is_a_terminal = No;
#ifdef TOStop
static int old_lword;
static int new_lword;
#endif

#define	STDIN	0
#define	STDOUT	1
#define	STDERR	2

/* This has to be defined as a subroutine for tputs (instead of a macro) */

static int
putstdout(TPUTS_PUTC_ARGTYPE ch)

{
    return putchar((int)ch);
}

void
screen_getsize()

{
    char *go;
#ifdef TIOCGWINSZ

    struct winsize ws;

    if (ioctl (1, TIOCGWINSZ, &ws) != -1)
    {
	if (ws.ws_row != 0)
	{
	    screen_length = ws.ws_row;
	}
	if (ws.ws_col != 0)
	{
	    screen_width = ws.ws_col - 1;
	}
    }

#else
#ifdef TIOCGSIZE

    struct ttysize ts;

    if (ioctl (1, TIOCGSIZE, &ts) != -1)
    {
	if (ts.ts_lines != 0)
	{
	    screen_length = ts.ts_lines;
	}
	if (ts.ts_cols != 0)
	{
	    screen_width = ts.ts_cols - 1;
	}
    }

#endif /* TIOCGSIZE */
#endif /* TIOCGWINSZ */

    if ((go = tgoto(tc_cursor_motion, 0, screen_length - 1)) != NULL)
	(void) strcpy(lower_left, go);
    else
	lower_left[0] = '\0';
}

int
screen_readtermcap(int interactive)

{
    char *bufptr;
    char *PCptr;
    char *term_name;
    char *go;
    int status;

    /* set defaults in case we aren't smart */
    screen_width = MAX_COLS;
    screen_length = 0;

    if (interactive == No)
    {
	/* pretend we have a dumb terminal */
	smart_terminal = No;
	return No;
    }

    /* assume we have a smart terminal until proven otherwise */
    smart_terminal = Yes;

    /* get the terminal name */
    term_name = getenv("TERM");

    /* if there is no TERM, assume it's a dumb terminal */
    /* patch courtesy of Sam Horrocks at telegraph.ics.uci.edu */
    if (term_name == NULL)
    {
	smart_terminal = No;
	return No;
    }

    /* now get the termcap entry */
    if ((status = tgetent(termcap_buf, term_name)) != 1)
    {
	if (status == -1)
	{
	    fprintf(stderr, "%s: can't open termcap file\n", myname);
	}
	else
	{
	    fprintf(stderr, "%s: no termcap entry for a `%s' terminal\n",
		    myname, term_name);
	}

	/* pretend it's dumb and proceed */
	smart_terminal = No;
	return No;
    }

    /* "hardcopy" immediately indicates a very stupid terminal */
    if (tgetflag("hc"))
    {
	smart_terminal = No;
	return No;
    }

    /* set up common terminal capabilities */
    if ((screen_length = tgetnum("li")) <= 0)
    {
	screen_length = 0;
    }

    /* screen_width is a little different */
    if ((screen_width = tgetnum("co")) == -1)
    {
	screen_width = 79;
    }
    else
    {
	screen_width -= 1;
    }

    /* terminals that overstrike need special attention */
    tc_overstrike = tgetflag("os");

    /* initialize the pointer into the termcap string buffer */
    bufptr = string_buffer;

    /* get "ce", clear to end */
    if (!tc_overstrike)
    {
	tc_clear_line = tgetstr("ce", &bufptr);
    }

    /* get necessary capabilities */
    if ((tc_clear_screen  = tgetstr("cl", &bufptr)) == NULL ||
	(tc_cursor_motion = tgetstr("cm", &bufptr)) == NULL)
    {
	smart_terminal = No;
	return No;
    }

    /* get some more sophisticated stuff -- these are optional */
    tc_clear_to_end   = tgetstr("cd", &bufptr);
    terminal_init  = tgetstr("ti", &bufptr);
    terminal_end   = tgetstr("te", &bufptr);
    tc_start_standout = tgetstr("so", &bufptr);
    tc_end_standout   = tgetstr("se", &bufptr);

    /* pad character */
    PC = (PCptr = tgetstr("pc", &bufptr)) ? *PCptr : 0;

    /* set convenience strings */
    if ((go = tgoto(tc_cursor_motion, 0, 0)) != NULL)
	(void) strcpy(home, go);
    else
	home[0] = '\0';
    /* (lower_left is set in screen_getsize) */

    /* get the actual screen size with an ioctl, if needed */
    /* This may change screen_width and screen_length, and it always
       sets lower_left. */
    screen_getsize();

    /* If screen_length is 0 from both termcap and ioctl then we are dumb */
    if (screen_length == 0)
    {
        smart_terminal = No;
        return No;
    }

    /* if stdout is not a terminal, pretend we are a dumb terminal */
#ifdef USE_SGTTY
    if (ioctl(STDOUT, TIOCGETP, &old_settings) == -1)
    {
	smart_terminal = No;
    }
#endif
#ifdef USE_TERMIO
    if (ioctl(STDOUT, TCGETA, &old_settings) == -1)
    {
	smart_terminal = No;
    }
#endif
#ifdef USE_TERMIOS
    if (tcgetattr(STDOUT, &old_settings) == -1)
    {
	smart_terminal = No;
    }
#endif

    return smart_terminal;
}

void
screen_init()

{
    /* get the old settings for safe keeping */
#ifdef USE_SGTTY
    if (ioctl(STDOUT, TIOCGETP, &old_settings) != -1)
    {
	/* copy the settings so we can modify them */
	new_settings = old_settings;

	/* turn on CBREAK and turn off character echo and tab expansion */
	new_settings.sg_flags |= CBREAK;
	new_settings.sg_flags &= ~(ECHO|XTABS);
	(void) ioctl(STDOUT, TIOCSETP, &new_settings);

	/* remember the erase and kill characters */
	ch_erase = old_settings.sg_erase;
	ch_kill  = old_settings.sg_kill;
	ch_werase  = old_settings.sg_werase;

#ifdef TOStop
	/* get the local mode word */
	(void) ioctl(STDOUT, TIOCLGET, &old_lword);

	/* modify it */
	new_lword = old_lword | LTOSTOP;
	(void) ioctl(STDOUT, TIOCLSET, &new_lword);
#endif
	/* remember that it really is a terminal */
	is_a_terminal = Yes;

	/* send the termcap initialization string */
	putcap(terminal_init);
    }
#endif
#ifdef USE_TERMIO
    if (ioctl(STDOUT, TCGETA, &old_settings) != -1)
    {
	/* copy the settings so we can modify them */
	new_settings = old_settings;

	/* turn off ICANON, character echo and tab expansion */
	new_settings.c_lflag &= ~(ICANON|ECHO);
	new_settings.c_oflag &= ~(TAB3);
	new_settings.c_cc[VMIN] = 1;
	new_settings.c_cc[VTIME] = 0;
	(void) ioctl(STDOUT, TCSETA, &new_settings);

	/* remember the erase and kill characters */
	ch_erase  = old_settings.c_cc[VERASE];
	ch_kill   = old_settings.c_cc[VKILL];
	ch_werase = old_settings.c_cc[VWERASE];

	/* remember that it really is a terminal */
	is_a_terminal = Yes;

	/* send the termcap initialization string */
	putcap(terminal_init);
    }
#endif
#ifdef USE_TERMIOS
    if (tcgetattr(STDOUT, &old_settings) != -1)
    {
	/* copy the settings so we can modify them */
	new_settings = old_settings;

	/* turn off ICANON, character echo and tab expansion */
	new_settings.c_lflag &= ~(ICANON|ECHO);
	new_settings.c_oflag &= ~(TAB3);
	new_settings.c_cc[VMIN] = 1;
	new_settings.c_cc[VTIME] = 0;
	(void) tcsetattr(STDOUT, TCSADRAIN, &new_settings);

	/* remember the erase and kill characters */
	ch_erase  = old_settings.c_cc[VERASE];
	ch_kill   = old_settings.c_cc[VKILL];
	ch_werase = old_settings.c_cc[VWERASE];

	/* remember that it really is a terminal */
	is_a_terminal = Yes;

	/* send the termcap initialization string */
	putcap(terminal_init);
    }
#endif

    if (!is_a_terminal)
    {
	/* not a terminal at all---consider it dumb */
	smart_terminal = No;
    }
}

void
screen_end()

{
    /* move to the lower left, clear the line and send "te" */
    if (smart_terminal)
    {
	putcap(lower_left);
	putcap(tc_clear_line);
	fflush(stdout);
	putcap(terminal_end);
    }

    /* if we have settings to reset, then do so */
    if (is_a_terminal)
    {
#ifdef USE_SGTTY
	(void) ioctl(STDOUT, TIOCSETP, &old_settings);
#ifdef TOStop
	(void) ioctl(STDOUT, TIOCLSET, &old_lword);
#endif
#endif
#ifdef USE_TERMIO
	(void) ioctl(STDOUT, TCSETA, &old_settings);
#endif
#ifdef USE_TERMIOS
	(void) tcsetattr(STDOUT, TCSADRAIN, &old_settings);
#endif
    }
}

void
screen_reinit()

{
    /* install our settings if it is a terminal */
    if (is_a_terminal)
    {
#ifdef USE_SGTTY
	(void) ioctl(STDOUT, TIOCSETP, &new_settings);
#ifdef TOStop
	(void) ioctl(STDOUT, TIOCLSET, &new_lword);
#endif
#endif
#ifdef USE_TERMIO
	(void) ioctl(STDOUT, TCSETA, &new_settings);
#endif
#ifdef USE_TERMIOS
	(void) tcsetattr(STDOUT, TCSADRAIN, &new_settings);
#endif
    }

    /* send init string */
    if (smart_terminal)
    {
	putcap(terminal_init);
    }
}

void
screen_move(int x, int y)

{
    char *go = tgoto(tc_cursor_motion, x, y);
    if (go)
	tputs(go, 1, putstdout);
}

void
screen_standout(const char *msg)

{
    if (smart_terminal)
    {
	putcap(tc_start_standout);
	fputs(msg, stdout);
	putcap(tc_end_standout);
    }
    else
    {
	fputs(msg, stdout);
    }
}

void
screen_clear(void)

{
    if (smart_terminal)
    {
	putcap(tc_clear_screen);
    }
}

int
screen_cte(void)

{
    if (smart_terminal)
    {
	if (tc_clear_to_end)
	{
	    putcap(tc_clear_to_end);
	    return(Yes);
	}
    }
    return(No);
}

void
screen_cleareol(int len)

{
    int i;

    if (smart_terminal && !tc_overstrike && len > 0)
    {
	if (tc_clear_line)
	{
	    putcap(tc_clear_line);
	    return;
	}
	else
	{
	    i = 0;
	    while (i++ < 0)
	    {
		putchar(' ');
	    }
	    i = 0;
	    while (i++ < 0)
	    {
		putchar('\b');
	    }
	    return;
	}
    }
    return;
}

void
screen_home(void)

{
    if (smart_terminal)
    {
	putcap(home);
    }
}


