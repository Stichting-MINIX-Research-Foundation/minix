/*	$NetBSD: screen.c,v 1.30 2015/07/07 22:53:25 nat Exp $	*/

/*-
 * Copyright (c) 1992, 1993
 *	The Regents of the University of California.  All rights reserved.
 *
 * This code is derived from software contributed to Berkeley by
 * Chris Torek and Darren F. Provine.
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
 *	@(#)screen.c	8.1 (Berkeley) 5/31/93
 */

/*
 * Tetris screen control.
 */

#include <sys/cdefs.h>
#include <sys/ioctl.h>

#include <setjmp.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <term.h>
#include <termios.h>
#include <unistd.h>

#ifndef sigmask
#define sigmask(s) (1 << ((s) - 1))
#endif

#include "screen.h"
#include "tetris.h"

static cell curscreen[B_SIZE];	/* 1 => standout (or otherwise marked) */
static int curscore;
static int isset;		/* true => terminal is in game mode */
static struct termios oldtt;
static void (*tstp)(int);

static	void	scr_stop(int);
static	void	stopset(int) __dead;


/*
 * Routine used by tputs().
 */
int
put(int c)
{

	return (putchar(c));
}

/*
 * putstr() is for unpadded strings (either as in termcap(5) or
 * simply literal strings); putpad() is for padded strings with
 * count=1.  (See screen.h for putpad().)
 */
#define	putstr(s)	(void)fputs(s, stdout)

static void
moveto(int r, int c)
{
	char *buf;

	buf = tiparm(cursor_address, r, c);
	if (buf != NULL)
		putpad(buf);
}

static void
setcolor(int c)
{
	char *buf;
	char monochrome[] = "\033[0m";
	if (nocolor == 1)
		return;
	if (set_a_foreground == NULL)
		return;

	if (c == 0 || c == 7)
		buf = monochrome;
	else
		buf = tiparm(set_a_foreground, c);
	if (buf != NULL)
		putpad(buf);
}

/*
 * Set up from termcap.
 */
void
scr_init(void)
{

	setupterm(NULL, 0, NULL);
	if (clear_screen == NULL)
		stop("cannot clear screen");
	if (cursor_address == NULL || cursor_up == NULL)
		stop("cannot do random cursor positioning");
}

/* this foolery is needed to modify tty state `atomically' */
static jmp_buf scr_onstop;

static void
stopset(int sig)
{
	sigset_t set;

	(void) signal(sig, SIG_DFL);
	(void) kill(getpid(), sig);
	sigemptyset(&set);
	sigaddset(&set, sig);
	(void) sigprocmask(SIG_UNBLOCK, &set, (sigset_t *)0);
	longjmp(scr_onstop, 1);
}

static void
scr_stop(int sig)
{
	sigset_t set;

	scr_end();
	(void) kill(getpid(), sig);
	sigemptyset(&set);
	sigaddset(&set, sig);
	(void) sigprocmask(SIG_UNBLOCK, &set, (sigset_t *)0);
	scr_set();
	scr_msg(key_msg, 1);
}

/*
 * Set up screen mode.
 */
void
scr_set(void)
{
	struct winsize ws;
	struct termios newtt;
	sigset_t nsigset, osigset;
	void (*ttou)(int);

	sigemptyset(&nsigset);
	sigaddset(&nsigset, SIGTSTP);
	sigaddset(&nsigset, SIGTTOU);
	(void) sigprocmask(SIG_BLOCK, &nsigset, &osigset);
	if ((tstp = signal(SIGTSTP, stopset)) == SIG_IGN)
		(void) signal(SIGTSTP, SIG_IGN);
	if ((ttou = signal(SIGTTOU, stopset)) == SIG_IGN)
		(void) signal(SIGTTOU, SIG_IGN);
	/*
	 * At last, we are ready to modify the tty state.  If
	 * we stop while at it, stopset() above will longjmp back
	 * to the setjmp here and we will start over.
	 */
	(void) setjmp(scr_onstop);
	(void) sigprocmask(SIG_SETMASK, &osigset, (sigset_t *)0);
	Rows = 0, Cols = 0;
	if (ioctl(0, TIOCGWINSZ, &ws) == 0) {
		Rows = ws.ws_row;
		Cols = ws.ws_col;
	}
	if (Rows == 0)
		Rows = lines;
	if (Cols == 0)
	    Cols = columns;
	if (Rows < MINROWS || Cols < MINCOLS) {
		(void) fprintf(stderr,
		    "the screen is too small: must be at least %dx%d, ",
		    MINCOLS, MINROWS);
		stop("");	/* stop() supplies \n */
	}
	if (tcgetattr(0, &oldtt) < 0)
		stop("tcgetattr() fails");
	newtt = oldtt;
	newtt.c_lflag &= ~(ICANON|ECHO);
	newtt.c_oflag &= ~OXTABS;
	if (tcsetattr(0, TCSADRAIN, &newtt) < 0)
		stop("tcsetattr() fails");
	ospeed = cfgetospeed(&newtt);
	(void) sigprocmask(SIG_BLOCK, &nsigset, &osigset);

	/*
	 * We made it.  We are now in screen mode, modulo TIstr
	 * (which we will fix immediately).
	 */
	if (enter_ca_mode)
		putstr(enter_ca_mode);
	if (cursor_invisible)
		putstr(cursor_invisible);
	if (tstp != SIG_IGN)
		(void) signal(SIGTSTP, scr_stop);
	if (ttou != SIG_IGN)
		(void) signal(SIGTTOU, ttou);

	isset = 1;
	(void) sigprocmask(SIG_SETMASK, &osigset, (sigset_t *)0);
	scr_clear();
}

/*
 * End screen mode.
 */
void
scr_end(void)
{
	sigset_t nsigset, osigset;

	sigemptyset(&nsigset);
	sigaddset(&nsigset, SIGTSTP);
	sigaddset(&nsigset, SIGTTOU);
	(void) sigprocmask(SIG_BLOCK, &nsigset, &osigset);
	/* move cursor to last line */
	if (cursor_to_ll)
		putstr(cursor_to_ll);
	else
		moveto(Rows - 1, 0);
	/* exit screen mode */
	if (exit_ca_mode)
		putstr(exit_ca_mode);
	if (cursor_normal)
		putstr(cursor_normal);
	(void) fflush(stdout);
	(void) tcsetattr(0, TCSADRAIN, &oldtt);
	isset = 0;
	/* restore signals */
	(void) signal(SIGTSTP, tstp);
	(void) sigprocmask(SIG_SETMASK, &osigset, (sigset_t *)0);
}

void
stop(const char *why)
{

	if (isset)
		scr_end();
	(void) fprintf(stderr, "aborting: %s\n", why);
	exit(1);
}

/*
 * Clear the screen, forgetting the current contents in the process.
 */
void
scr_clear(void)
{

	putpad(clear_screen);
	curscore = -1;
	memset((char *)curscreen, 0, sizeof(curscreen));
}

#if vax && !__GNUC__
typedef int regcell;	/* pcc is bad at `register char', etc */
#else
typedef cell regcell;
#endif

/*
 * Update the screen.
 */
void
scr_update(void)
{
	cell *bp, *sp;
	regcell so, cur_so = 0;
	int i, ccol, j;
	sigset_t nsigset, osigset;
	static const struct shape *lastshape;

	sigemptyset(&nsigset);
	sigaddset(&nsigset, SIGTSTP);
	(void) sigprocmask(SIG_BLOCK, &nsigset, &osigset);

	/* always leave cursor after last displayed point */
	curscreen[D_LAST * B_COLS - 1] = -1;

	if (score != curscore) {
		if (cursor_home)
			putpad(cursor_home);
		else
			moveto(0, 0);
		setcolor(0);
		(void) printf("Score: %d", score);
		curscore = score;
	}

	/* draw preview of nextpattern */
	if (showpreview && (nextshape != lastshape)) {
		static int r=5, c=2;
		int tr, tc, t; 

		lastshape = nextshape;
		
		/* clean */
		putpad(exit_standout_mode);
		moveto(r-1, c-1); putstr("          ");
		moveto(r,   c-1); putstr("          ");
		moveto(r+1, c-1); putstr("          ");
		moveto(r+2, c-1); putstr("          ");

		moveto(r-3, c-2);
		putstr("Next shape:");
						
		/* draw */
		putpad(enter_standout_mode);
		setcolor(nextshape->color);
		moveto(r, 2*c);
		putstr("  ");
		for(i=0; i<3; i++) {
			t = c + r*B_COLS;
			t += nextshape->off[i];

			tr = t / B_COLS;
			tc = t % B_COLS;

			moveto(tr, 2*tc);
			putstr("  ");
		}
		putpad(exit_standout_mode);
	}
	
	bp = &board[D_FIRST * B_COLS];
	sp = &curscreen[D_FIRST * B_COLS];
	for (j = D_FIRST; j < D_LAST; j++) {
		ccol = -1;
		for (i = 0; i < B_COLS; bp++, sp++, i++) {
			if (*sp == (so = *bp))
				continue;
			*sp = so;
			if (i != ccol) {
				if (cur_so && move_standout_mode) {
					putpad(exit_standout_mode);
					cur_so = 0;
				}
				moveto(RTOD(j), CTOD(i));
			}
			if (enter_standout_mode) {
				if (so != cur_so) {
					setcolor(so);
					putpad(so ?
					    enter_standout_mode :
					    exit_standout_mode);
					cur_so = so;
				}
#ifdef DEBUG
				char buf[3];
				snprintf(buf, sizeof(buf), "%d%d", so, so);
				putstr(buf);
#else
				putstr("  ");
#endif
			} else
				putstr(so ? "XX" : "  ");
			ccol = i + 1;
			/*
			 * Look ahead a bit, to avoid extra motion if
			 * we will be redrawing the cell after the next.
			 * Motion probably takes four or more characters,
			 * so we save even if we rewrite two cells
			 * `unnecessarily'.  Skip it all, though, if
			 * the next cell is a different color.
			 */
#define	STOP (B_COLS - 3)
			if (i > STOP || sp[1] != bp[1] || so != bp[1])
				continue;
			if (sp[2] != bp[2])
				sp[1] = -1;
			else if (i < STOP && so == bp[2] && sp[3] != bp[3]) {
				sp[2] = -1;
				sp[1] = -1;
			}
		}
	}
	if (cur_so)
		putpad(exit_standout_mode);
	(void) fflush(stdout);
	(void) sigprocmask(SIG_SETMASK, &osigset, (sigset_t *)0);
}

/*
 * Write a message (set!=0), or clear the same message (set==0).
 * (We need its length in case we have to overwrite with blanks.)
 */
void
scr_msg(char *s, int set)
{
	
	if (set || clr_eol == NULL) {
		int l = strlen(s);

		moveto(Rows - 2, ((Cols - l) >> 1) - 1);
		if (set)
			putstr(s);
		else
			while (--l >= 0)
				(void) putchar(' ');
	} else {
		moveto(Rows - 2, 0);
		putpad(clr_eol);
	}
}
