/*	$NetBSD: tty.c,v 1.43 2011/08/29 11:07:38 christos Exp $	*/

/*-
 * Copyright (c) 1992, 1993, 1994
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
static char sccsid[] = "@(#)tty.c	8.6 (Berkeley) 1/10/95";
#else
__RCSID("$NetBSD: tty.c,v 1.43 2011/08/29 11:07:38 christos Exp $");
#endif
#endif				/* not lint */

#include <sys/types.h>

#include <stdlib.h>
#include <termios.h>
#include <unistd.h>
#include <sys/fcntl.h>
#include <sys/ioctl.h>

#include "curses.h"
#include "curses_private.h"

/*
 * In general, curses should leave tty hardware settings alone (speed, parity,
 * word size).  This is most easily done in BSD by using TCSASOFT on all
 * tcsetattr calls.  On other systems, it would be better to get and restore
 * those attributes at each change, or at least when stopped and restarted.
 * See also the comments in getterm().
 */
#ifndef TCSASOFT
#define	TCSASOFT	0
#endif

int __tcaction = TCSASOFT != 0;		/* Ignore hardware settings */

#ifndef	OXTABS
#ifdef	XTABS			/* SMI uses XTABS. */
#define	OXTABS	XTABS
#else
#define	OXTABS	0
#endif
#endif

/*
 * baudrate --
 *	Return the current baudrate
 */
int
baudrate(void)
{
	if (_cursesi_screen->notty == TRUE)
		return 0;

	return cfgetospeed(&_cursesi_screen->baset);
}

/*
 * gettmode --
 *	Do terminal type initialization.
 */
int
gettmode(void)
{
	if (_cursesi_gettmode(_cursesi_screen) == ERR)
		return ERR;

	__GT = _cursesi_screen->GT;
	__NONL = _cursesi_screen->NONL;
	return OK;
}

/*
 * _cursesi_gettmode --
 *      Do the terminal type initialisation for the tty attached to the
 *  given screen.
 */
int
_cursesi_gettmode(SCREEN *screen)
{
	screen->useraw = 0;

	if (tcgetattr(fileno(screen->infd), &screen->orig_termios)) {
		/* if the input fd is not a tty try the output */
		if (tcgetattr(fileno(screen->infd), &screen->orig_termios)) {
			/* not a tty ... we will disable tty related stuff */
			screen->notty = TRUE;
			__GT = 0;
			__NONL = 0;
			return OK;
		}
	}

	screen->baset = screen->orig_termios;
	screen->baset.c_oflag &= ~OXTABS;

	screen->GT = 0;	/* historical. was used before we wired OXTABS off */
	screen->NONL = (screen->baset.c_oflag & ONLCR) == 0;

	/*
	 * XXX
	 * System V and SMI systems overload VMIN and VTIME, such that
	 * VMIN is the same as the VEOF element, and VTIME is the same
	 * as the VEOL element.  This means that, if VEOF was ^D, the
	 * default VMIN is 4.  Majorly stupid.
	 */
	screen->cbreakt = screen->baset;
	screen->cbreakt.c_lflag &= ~(ECHO | ECHONL | ICANON);
	screen->cbreakt.c_cc[VMIN] = 1;
	screen->cbreakt.c_cc[VTIME] = 0;

	screen->rawt = screen->cbreakt;
	screen->rawt.c_iflag &= ~(IGNBRK | BRKINT | PARMRK | INLCR | IGNCR |
				  ICRNL | IXON);
	screen->rawt.c_oflag &= ~OPOST;
	screen->rawt.c_lflag &= ~(ISIG | IEXTEN);

#if TCSASOFT == 0
	/*
	 * In general, curses should leave hardware-related settings alone.
	 * This includes parity and word size.  Older versions set the tty
	 * to 8 bits, no parity in raw(), but this is considered to be an
	 * artifact of the old tty interface.  If it's desired to change
	 * parity and word size, the TCSASOFT bit has to be removed from the
	 * calls that switch to/from "raw" mode.
	 */
	screen->rawt.c_iflag &= ~ISTRIP;
	screen->rawt.c_cflag &= ~(CSIZE | PARENB);
	screen->rawt.c_cflag |= CS8;
#endif

	screen->curt = &screen->baset;
	return tcsetattr(fileno(screen->infd), TCSASOFT | TCSADRAIN,
	    screen->curt) ? ERR : OK;
}

/*
 * raw --
 *	Put the terminal into raw mode
 */
int
raw(void)
{
#ifdef DEBUG
	__CTRACE(__CTRACE_MISC, "raw()\n");
#endif
	/* Check if we need to restart ... */
	if (_cursesi_screen->endwin)
		__restartwin();

	_cursesi_screen->useraw = __pfast = __rawmode = 1;
	_cursesi_screen->curt = &_cursesi_screen->rawt;
	if (_cursesi_screen->notty == TRUE)
		return OK;
	return tcsetattr(fileno(_cursesi_screen->infd), TCSASOFT | TCSADRAIN,
	    _cursesi_screen->curt) ? ERR : OK;
}

/*
 * noraw --
 *	Put the terminal into cooked mode
 */
int
noraw(void)
{
#ifdef DEBUG
	__CTRACE(__CTRACE_MISC, "noraw()\n");
#endif
	/* Check if we need to restart ... */
	if (_cursesi_screen->endwin)
		__restartwin();

	_cursesi_screen->useraw = __pfast = __rawmode = 0;
	if (_cursesi_screen->notty == TRUE)
		return OK;
	_cursesi_screen->curt = &_cursesi_screen->baset;
	return tcsetattr(fileno(_cursesi_screen->infd), TCSASOFT | TCSADRAIN,
	    _cursesi_screen->curt) ? ERR : OK;
}

/*
 * cbreak --
 * 	Enable cbreak mode
 */
int
cbreak(void)
{
#ifdef DEBUG
	__CTRACE(__CTRACE_MISC, "cbreak()\n");
#endif
	/* Check if we need to restart ... */
	if (_cursesi_screen->endwin)
		__restartwin();

	__rawmode = 1;
	if (_cursesi_screen->notty == TRUE)
		return OK;
	_cursesi_screen->curt = _cursesi_screen->useraw ?
		&_cursesi_screen->rawt : &_cursesi_screen->cbreakt;
	return tcsetattr(fileno(_cursesi_screen->infd), TCSASOFT | TCSADRAIN,
	    _cursesi_screen->curt) ? ERR : OK;
}

/*
 * nocbreak --
 *	Disable cbreak mode
 */
int
nocbreak(void)
{
#ifdef DEBUG
	__CTRACE(__CTRACE_MISC, "nocbreak()\n");
#endif
	/* Check if we need to restart ... */
	if (_cursesi_screen->endwin)
		__restartwin();

	__rawmode = 0;
	if (_cursesi_screen->notty == TRUE)
		return OK;
	  /* if we were in halfdelay mode then nuke the timeout */
	if ((_cursesi_screen->half_delay == TRUE) &&
	    (__notimeout() == ERR))
		return ERR;

	_cursesi_screen->half_delay = FALSE;
	_cursesi_screen->curt = _cursesi_screen->useraw ?
		&_cursesi_screen->rawt : &_cursesi_screen->baset;
	return tcsetattr(fileno(_cursesi_screen->infd), TCSASOFT | TCSADRAIN,
	    _cursesi_screen->curt) ? ERR : OK;
}

/*
 * halfdelay --
 *    Put the terminal into cbreak mode with the specified timeout.
 *
 */
int
halfdelay(int duration)
{
	if ((duration < 1) || (duration > 255))
		return ERR;

	if (cbreak() == ERR)
		return ERR;

	if (__timeout(duration) == ERR)
		return ERR;

	_cursesi_screen->half_delay = TRUE;
	return OK;
}
	
int
__delay(void)
 {
#ifdef DEBUG
	__CTRACE(__CTRACE_MISC, "__delay()\n");
#endif
	/* Check if we need to restart ... */
	if (_cursesi_screen->endwin)
		__restartwin();

	if (_cursesi_screen->notty == TRUE)
		return OK;
	_cursesi_screen->rawt.c_cc[VMIN] = 1;
	_cursesi_screen->rawt.c_cc[VTIME] = 0;
	_cursesi_screen->cbreakt.c_cc[VMIN] = 1;
	_cursesi_screen->cbreakt.c_cc[VTIME] = 0;
	_cursesi_screen->baset.c_cc[VMIN] = 1;
	_cursesi_screen->baset.c_cc[VTIME] = 0;

	if (tcsetattr(fileno(_cursesi_screen->infd), TCSASOFT | TCSANOW, 
	    _cursesi_screen->curt)) {
		__restore_termios();
		return ERR;
	}

	return OK;
}

int
__nodelay(void)
{
#ifdef DEBUG
	__CTRACE(__CTRACE_MISC, "__nodelay()\n");
#endif
	/* Check if we need to restart ... */
	if (_cursesi_screen->endwin)
		__restartwin();

	if (_cursesi_screen->notty == TRUE)
		return OK;
	_cursesi_screen->rawt.c_cc[VMIN] = 0;
	_cursesi_screen->rawt.c_cc[VTIME] = 0;
	_cursesi_screen->cbreakt.c_cc[VMIN] = 0;
	_cursesi_screen->cbreakt.c_cc[VTIME] = 0;
	_cursesi_screen->baset.c_cc[VMIN] = 0;
	_cursesi_screen->baset.c_cc[VTIME] = 0;

	if (tcsetattr(fileno(_cursesi_screen->infd), TCSASOFT | TCSANOW,
	    _cursesi_screen->curt)) {
		__restore_termios();
		return ERR;
	}

	return OK;
}

void
__save_termios(void)
{
	/* Check if we need to restart ... */
	if (_cursesi_screen->endwin)
		__restartwin();

	if (_cursesi_screen->notty == TRUE)
		return;
	_cursesi_screen->ovmin = _cursesi_screen->cbreakt.c_cc[VMIN];
	_cursesi_screen->ovtime = _cursesi_screen->cbreakt.c_cc[VTIME];
}

void
__restore_termios(void)
{
	/* Check if we need to restart ... */
	if (_cursesi_screen->endwin)
		__restartwin();

	if (_cursesi_screen->notty == TRUE)
		return;
	_cursesi_screen->rawt.c_cc[VMIN] = _cursesi_screen->ovmin;
	_cursesi_screen->rawt.c_cc[VTIME] = _cursesi_screen->ovtime;
	_cursesi_screen->cbreakt.c_cc[VMIN] = _cursesi_screen->ovmin;
	_cursesi_screen->cbreakt.c_cc[VTIME] = _cursesi_screen->ovtime;
	_cursesi_screen->baset.c_cc[VMIN] = _cursesi_screen->ovmin;
	_cursesi_screen->baset.c_cc[VTIME] = _cursesi_screen->ovtime;
}

int
__timeout(int delay)
{
#ifdef DEBUG
	__CTRACE(__CTRACE_MISC, "__timeout()\n");
#endif
	/* Check if we need to restart ... */
	if (_cursesi_screen->endwin)
		__restartwin();

	if (_cursesi_screen->notty == TRUE)
		return OK;
	_cursesi_screen->ovmin = _cursesi_screen->cbreakt.c_cc[VMIN];
	_cursesi_screen->ovtime = _cursesi_screen->cbreakt.c_cc[VTIME];
	_cursesi_screen->rawt.c_cc[VMIN] = 0;
	_cursesi_screen->rawt.c_cc[VTIME] = delay;
	_cursesi_screen->cbreakt.c_cc[VMIN] = 0;
	_cursesi_screen->cbreakt.c_cc[VTIME] = delay;
	_cursesi_screen->baset.c_cc[VMIN] = 0;
	_cursesi_screen->baset.c_cc[VTIME] = delay;

	if (tcsetattr(fileno(_cursesi_screen->infd), TCSASOFT | TCSANOW,
	    _cursesi_screen->curt)) {
		__restore_termios();
		return ERR;
	}

	return OK;
}

int
__notimeout(void)
{
#ifdef DEBUG
	__CTRACE(__CTRACE_MISC, "__notimeout()\n");
#endif
	/* Check if we need to restart ... */
	if (_cursesi_screen->endwin)
		__restartwin();

	if (_cursesi_screen->notty == TRUE)
		return OK;
	_cursesi_screen->rawt.c_cc[VMIN] = 1;
	_cursesi_screen->rawt.c_cc[VTIME] = 0;
	_cursesi_screen->cbreakt.c_cc[VMIN] = 1;
	_cursesi_screen->cbreakt.c_cc[VTIME] = 0;
	_cursesi_screen->baset.c_cc[VMIN] = 1;
	_cursesi_screen->baset.c_cc[VTIME] = 0;

	return tcsetattr(fileno(_cursesi_screen->infd), TCSASOFT | TCSANOW,
	    _cursesi_screen->curt) ? ERR : OK;
}

int
echo(void)
{
#ifdef DEBUG
	__CTRACE(__CTRACE_MISC, "echo()\n");
#endif
	/* Check if we need to restart ... */
	if (_cursesi_screen->endwin)
		__restartwin();

	__echoit = 1;
	return OK;
}

int
noecho(void)
{
#ifdef DEBUG
	__CTRACE(__CTRACE_MISC, "noecho()\n");
#endif
	/* Check if we need to restart ... */
	if (_cursesi_screen->endwin)
		__restartwin();

	__echoit = 0;
	return OK;
}

int
nl(void)
{
#ifdef DEBUG
	__CTRACE(__CTRACE_MISC, "nl()\n");
#endif
	/* Check if we need to restart ... */
	if (_cursesi_screen->endwin)
		__restartwin();

	if (_cursesi_screen->notty == TRUE)
		return OK;
	_cursesi_screen->rawt.c_iflag |= ICRNL;
	_cursesi_screen->rawt.c_oflag |= ONLCR;
	_cursesi_screen->cbreakt.c_iflag |= ICRNL;
	_cursesi_screen->cbreakt.c_oflag |= ONLCR;
	_cursesi_screen->baset.c_iflag |= ICRNL;
	_cursesi_screen->baset.c_oflag |= ONLCR;

	_cursesi_screen->nl = 1;
	_cursesi_screen->pfast = _cursesi_screen->rawmode;
	return tcsetattr(fileno(_cursesi_screen->infd), TCSASOFT | TCSADRAIN,
	    _cursesi_screen->curt) ? ERR : OK;
}

int
nonl(void)
{
#ifdef DEBUG
	__CTRACE(__CTRACE_MISC, "nonl()\n");
#endif
	/* Check if we need to restart ... */
	if (_cursesi_screen->endwin)
		__restartwin();

	if (_cursesi_screen->notty == TRUE)
		return OK;
	_cursesi_screen->rawt.c_iflag &= ~ICRNL;
	_cursesi_screen->rawt.c_oflag &= ~ONLCR;
	_cursesi_screen->cbreakt.c_iflag &= ~ICRNL;
	_cursesi_screen->cbreakt.c_oflag &= ~ONLCR;
	_cursesi_screen->baset.c_iflag &= ~ICRNL;
	_cursesi_screen->baset.c_oflag &= ~ONLCR;

	_cursesi_screen->nl = 0;
	__pfast = 1;
	return tcsetattr(fileno(_cursesi_screen->infd), TCSASOFT | TCSADRAIN,
	    _cursesi_screen->curt) ? ERR : OK;
}

#ifndef _CURSES_USE_MACROS
void
noqiflush(void)
{
	(void) intrflush(stdscr, FALSE);
}

void
qiflush(void)
{
	(void) intrflush(stdscr, TRUE);
}
#endif	/* _CURSES_USE_MACROS */

int
intrflush(WINDOW *win, bool bf)	/*ARGSUSED*/
{
	/* Check if we need to restart ... */
	if (_cursesi_screen->endwin)
		__restartwin();

	if (_cursesi_screen->notty == TRUE)
		return OK;
	if (bf) {
		_cursesi_screen->rawt.c_lflag &= ~NOFLSH;
		_cursesi_screen->cbreakt.c_lflag &= ~NOFLSH;
		_cursesi_screen->baset.c_lflag &= ~NOFLSH;
	} else {
		_cursesi_screen->rawt.c_lflag |= NOFLSH;
		_cursesi_screen->cbreakt.c_lflag |= NOFLSH;
		_cursesi_screen->baset.c_lflag |= NOFLSH;
	}

	__pfast = 1;
	return tcsetattr(fileno(_cursesi_screen->infd), TCSASOFT | TCSADRAIN,
	    _cursesi_screen->curt) ? ERR : OK;
}

void
__startwin(SCREEN *screen)
{

	(void) fflush(screen->infd);

	/*
	 * Some C libraries default to a 1K buffer when talking to a tty.
	 * With a larger screen, especially across a network, we'd like
	 * to get it to all flush in a single write.  Make it twice as big
	 * as just the characters (so that we have room for cursor motions
	 * and attribute information) but no more than 8K.
	 */
	if (screen->stdbuf == NULL) {
		screen->len = LINES * COLS * 2;
		if (screen->len > 8192)
			screen->len = 8192;
		if ((screen->stdbuf = malloc(screen->len)) == NULL)
			screen->len = 0;
	}
	(void) setvbuf(screen->outfd, screen->stdbuf, _IOFBF, screen->len);

	ti_puts(screen->term, t_enter_ca_mode(screen->term), 0,
		__cputchar_args, (void *) screen->outfd);
	ti_puts(screen->term, t_cursor_normal(screen->term), 0,
	    __cputchar_args, (void *) screen->outfd);
	if (screen->curscr->flags & __KEYPAD)
		ti_puts(screen->term, t_keypad_xmit(screen->term), 0,
		    __cputchar_args, (void *) screen->outfd);
	screen->endwin = 0;
}

int
endwin(void)
{
#ifdef DEBUG
	__CTRACE(__CTRACE_MISC, "endwin\n");
#endif
	return __stopwin();
}

bool
isendwin(void)
{
	return _cursesi_screen->endwin ? TRUE : FALSE;
}

int
flushinp(void)
{
	(void) fpurge(_cursesi_screen->infd);
	return OK;
}

/*
 * The following routines, savetty and resetty are completely useless and
 * are left in only as stubs.  If people actually use them they will almost
 * certainly screw up the state of the world.
 */
/*static struct termios savedtty;*/
int
savetty(void)
{
	if (_cursesi_screen->notty == TRUE)
		return OK;
	return tcgetattr(fileno(_cursesi_screen->infd),
	    &_cursesi_screen->savedtty) ? ERR : OK;
}

int
resetty(void)
{
	if (_cursesi_screen->notty == TRUE)
		return OK;
	return tcsetattr(fileno(_cursesi_screen->infd), TCSASOFT | TCSADRAIN,
	    &_cursesi_screen->savedtty) ? ERR : OK;
}

/*
 * erasechar --
 *     Return the character of the erase key.
 */
char
erasechar(void)
{
	if (_cursesi_screen->notty == TRUE)
		return 0;
	return _cursesi_screen->baset.c_cc[VERASE];
}

/*
 * killchar --
 *     Return the character of the kill key.
 */
char
killchar(void)
{
	if (_cursesi_screen->notty == TRUE)
		return 0;
	return _cursesi_screen->baset.c_cc[VKILL];
}

/*
 * erasewchar --
 *     Return the wide character of the erase key.
 */
int
erasewchar( wchar_t *ch )
{
#ifndef HAVE_WCHAR
	return ERR;
#else
	if (_cursesi_screen->notty == TRUE)
		return ERR;
	*ch = _cursesi_screen->baset.c_cc[VERASE];
	return OK;
#endif /* HAVE_WCHAR */
}

/*
 * killwchar --
 *     Return the wide character of the kill key.
 */
int
killwchar( wchar_t *ch )
{
#ifndef HAVE_WCHAR
	return ERR;
#else
	if (_cursesi_screen->notty == TRUE)
		return 0;
	*ch = _cursesi_screen->baset.c_cc[VKILL];
	return OK;
#endif /* HAVE_WCHAR */
}
