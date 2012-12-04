/*	$NetBSD: tstp.c,v 1.39 2011/08/29 11:07:38 christos Exp $	*/

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
static char sccsid[] = "@(#)tstp.c	8.3 (Berkeley) 5/4/94";
#else
__RCSID("$NetBSD: tstp.c,v 1.39 2011/08/29 11:07:38 christos Exp $");
#endif
#endif				/* not lint */

#include <sys/ioctl.h>

#include <errno.h>
#include <signal.h>
#include <termios.h>
#include <unistd.h>

#include "curses.h"
#include "curses_private.h"

static int tstp_set = 0;
static int winch_set = 0;

static void (*otstpfn)
__P((int)) = SIG_DFL;

static struct sigaction	owsa;
#ifndef TCSASOFT
#define TCSASOFT 0
#endif

/*
 * stop_signal_handler --
 *	Handle stop signals.
 */
void
__stop_signal_handler(/*ARGSUSED*/int signo)
{
	sigset_t oset, set;

	/*
	 * Block window change and timer signals.  The latter is because
	 * applications use timers to decide when to repaint the screen.
	 */
	(void) sigemptyset(&set);
	(void) sigaddset(&set, SIGALRM);
	(void) sigaddset(&set, SIGWINCH);
	(void) sigprocmask(SIG_BLOCK, &set, &oset);

	/*
	 * End the window, which also resets the terminal state to the
	 * original modes.
	 */
	__stopwin();

	/* Unblock SIGTSTP. */
	(void) sigemptyset(&set);
	(void) sigaddset(&set, SIGTSTP);
	(void) sigprocmask(SIG_UNBLOCK, &set, NULL);

	/* Stop ourselves. */
	(void) kill(0, SIGTSTP);

	/* Time passes ... */

	/* restart things */
	__restartwin();

	/* Reset the signals. */
	(void) sigprocmask(SIG_SETMASK, &oset, NULL);
}

/*
 * Set the TSTP handler.
 */
void
__set_stophandler(void)
{
#ifdef DEBUG
	__CTRACE(__CTRACE_MISC, "__set_stophandler: %d\n", tstp_set);
#endif
	if (!tstp_set) {
		otstpfn = signal(SIGTSTP, __stop_signal_handler);
		tstp_set = 1;
	}
}

/*
 * Restore the TSTP handler.
 */
void
__restore_stophandler(void)
{
#ifdef DEBUG
	__CTRACE(__CTRACE_MISC, "__restore_stophandler: %d\n", tstp_set);
#endif
	if (tstp_set) {
		(void) signal(SIGTSTP, otstpfn);
		tstp_set = 0;
	}
}

/*
 * winch_signal_handler --
 *	Handle winch signals by pushing KEY_RESIZE into the input stream.
 */
void
__winch_signal_handler(/*ARGSUSED*/int signo)
{
	struct winsize win;

	if (ioctl(fileno(_cursesi_screen->outfd), TIOCGWINSZ, &win) != -1 &&
	    win.ws_row != 0 && win.ws_col != 0) {
		LINES = win.ws_row;
		COLS = win.ws_col;
	}
	/*
	 * If there was a previous handler, call that,
	 * otherwise tell getch() to send KEY_RESIZE.
	 */
	if (owsa.sa_handler !=  NULL)
		owsa.sa_handler(signo);
	else
		_cursesi_screen->resized = 1;
}

/*
 * Set the WINCH handler.
 */
void
__set_winchhandler(void)
{
#ifdef DEBUG
	__CTRACE(__CTRACE_MISC, "__set_winchhandler: %d\n", winch_set);
#endif
	if (!winch_set) {
		struct sigaction sa;

		sa.sa_handler = __winch_signal_handler;
		sa.sa_flags = 0;
		sigemptyset(&sa.sa_mask);
		sigaction(SIGWINCH, &sa, &owsa);
		winch_set = 1;
#ifdef DEBUG
		__CTRACE(__CTRACE_MISC,
		    "__set_winchhandler: owsa.sa_handler=%p\n",
		    owsa.sa_handler);
#endif
	}
}

/*
 * Restore the WINCH handler.
 */
void
__restore_winchhandler(void)
{
#ifdef DEBUG
	__CTRACE(__CTRACE_MISC, "__restore_winchhandler: %d\n", winch_set);
#endif
	if (winch_set > 0) {
		struct sigaction cwsa;

		sigaction(SIGWINCH, NULL, &cwsa);
		if (cwsa.sa_handler == owsa.sa_handler) {
			sigaction(SIGWINCH, &owsa, NULL);
			winch_set = 0;
		} else {
			/*
			 * We're now using the programs WINCH handler,
			 * so don't restore the previous one.
			 */
			winch_set = -1;
#ifdef DEBUG
			__CTRACE(__CTRACE_MISC, "cwsa.sa_handler = %p\n",
			    cwsa.sa_handler);
#endif
		}
	}
}

/* To allow both SIGTSTP and endwin() to come back nicely, we provide
   the following routines. */

int
__stopwin(void)
{
#ifdef DEBUG
	__CTRACE(__CTRACE_MISC, "__stopwin\n");
#endif
	if (_cursesi_screen->endwin)
		return OK;

	/* Get the current terminal state (which the user may have changed). */
	(void) tcgetattr(fileno(_cursesi_screen->infd),
			 &_cursesi_screen->save_termios);

	__restore_stophandler();
	__restore_winchhandler();

	if (curscr != NULL) {
		__unsetattr(0);
		__mvcur((int) curscr->cury, (int) curscr->curx,
		    (int) curscr->maxy - 1, 0, 0);
	}

	if (meta_off != NULL)
		(void) tputs(meta_off, 0, __cputchar);

	if ((curscr != NULL) && (curscr->flags & __KEYPAD))
		(void) tputs(keypad_local, 0, __cputchar);
	(void) tputs(cursor_normal, 0, __cputchar);
	(void) tputs(exit_ca_mode, 0, __cputchar);
	(void) fflush(_cursesi_screen->outfd);
	(void) setvbuf(_cursesi_screen->outfd, NULL, _IOLBF, (size_t) 0);

	_cursesi_screen->endwin = 1;

	return tcsetattr(fileno(_cursesi_screen->infd), TCSASOFT | TCSADRAIN,
	    &_cursesi_screen->orig_termios) ? ERR : OK;
}


void
__restartwin(void)
{
	struct winsize win;
	int nlines, ncols;

#ifdef DEBUG
	__CTRACE(__CTRACE_MISC, "__restartwin\n");
#endif
	if (!_cursesi_screen->endwin)
		return;

	/* Reset the curses SIGTSTP and SIGWINCH signal handlers. */
	__set_stophandler();
	__set_winchhandler();

	/*
	 * Check to see if the window size has changed.
	 * If the application didn't update LINES and COLS,
	 * set the * resized flag to tell getch() to push KEY_RESIZE.
	 * Update curscr (which also updates __virtscr) and stdscr
	 * to match the new size.
	 */
	if (ioctl(fileno(_cursesi_screen->outfd), TIOCGWINSZ, &win) != -1 &&
	    win.ws_row != 0 && win.ws_col != 0) {
		if (win.ws_row != LINES) {
			LINES = win.ws_row;
			_cursesi_screen->resized = 1;
		}
		if (win.ws_col != COLS) {
			COLS = win.ws_col;
			_cursesi_screen->resized = 1;
		}
	}
	/*
	 * We need to make local copies of LINES and COLS, otherwise we
	 * could lose if they are changed between wresize() calls.
	 */
	nlines = LINES;
	ncols = COLS;
	if (curscr->maxy != nlines || curscr->maxx != ncols)
		wresize(curscr, nlines, ncols);
	if (stdscr->maxy != nlines || stdscr->maxx != ncols)
		wresize(stdscr, nlines, ncols);

	/* save the new "default" terminal state */
	(void) tcgetattr(fileno(_cursesi_screen->infd),
			 &_cursesi_screen->orig_termios);

	/* Reset the terminal state to the mode just before we stopped. */
	(void) tcsetattr(fileno(_cursesi_screen->infd), TCSASOFT | TCSADRAIN,
	    &_cursesi_screen->save_termios);

	/* Restore colours */
	__restore_colors();

	/* Reset meta */
	__restore_meta_state();

	/* Restart the screen. */
	__startwin(_cursesi_screen);

	/* Reset cursor visibility */
	__restore_cursor_vis();

	/* Repaint the screen. */
	wrefresh(curscr);
}

int
def_prog_mode(void)
{
	if (_cursesi_screen->endwin)
		return ERR;

	return tcgetattr(fileno(_cursesi_screen->infd),
	    &_cursesi_screen->save_termios) ? ERR : OK;
}

int
reset_prog_mode(void)
{

	return tcsetattr(fileno(_cursesi_screen->infd), TCSASOFT | TCSADRAIN,
	    &_cursesi_screen->save_termios) ? ERR : OK;
}

int
def_shell_mode(void)
{
	return tcgetattr(fileno(_cursesi_screen->infd),
	    &_cursesi_screen->orig_termios) ? ERR : OK;
}

int
reset_shell_mode(void)
{
	return __stopwin();
}
