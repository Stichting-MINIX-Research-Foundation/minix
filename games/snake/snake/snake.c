/*	$NetBSD: snake.c,v 1.28 2012/06/19 05:46:09 dholland Exp $	*/

/*
 * Copyright (c) 1980, 1993
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
__COPYRIGHT("@(#) Copyright (c) 1980, 1993\
 The Regents of the University of California.  All rights reserved.");
#endif				/* not lint */

#ifndef lint
#if 0
static char sccsid[] = "@(#)snake.c	8.2 (Berkeley) 1/7/94";
#else
__RCSID("$NetBSD: snake.c,v 1.28 2012/06/19 05:46:09 dholland Exp $");
#endif
#endif				/* not lint */

/*
 * snake - crt hack game.
 *
 * You move around the screen with arrow keys trying to pick up money
 * without getting eaten by the snake.  hjkl work as in vi in place of
 * arrow keys.  You can leave at the exit any time.
 *
 * compile as follows:
 *	cc -O snake.c move.c -o snake -lm -ltermlib
 */

#include <sys/param.h>

#include <curses.h>
#include <fcntl.h>
#include <pwd.h>
#include <time.h>
#include <unistd.h>
#include <sys/types.h>
#include <err.h>
#include <math.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <termios.h>

#include "pathnames.h"

#define cashvalue	chunk*(loot-penalty)/25

struct point {
	int col, line;
};

#define	same(s1, s2)	((s1)->line == (s2)->line && (s1)->col == (s2)->col)

#define PENALTY  10		/* % penalty for invoking spacewarp	 */

#define EOT	'\004'
#define LF	'\n'
#define DEL	'\177'

#define ME		'I'
#define SNAKEHEAD	'S'
#define SNAKETAIL	's'
#define TREASURE	'$'
#define GOAL		'#'

#ifndef MIN
#define MIN(a, b) ((a) < (b) ? (a) : (b))
#endif

#define pchar(point, c)	mvaddch((point)->line + 1, (point)->col + 1, (c))
#define delay(t)	usleep(t * 50000);

static struct point you;
static struct point money;
static struct point finish;
static struct point snake[6];

static int loot, penalty;
static int moves;
static int fast = 1;

static int rawscores;
static FILE *logfile;

static int lcnt, ccnt;		/* user's idea of screen size */
static int chunk;		/* amount of money given at a time */

static void chase(struct point *, struct point *);
static int chk(const struct point *);
static void drawbox(void);
static void flushi(void);
static void length(int);
static void logit(const char *);
static void mainloop(void) __dead;
static struct point *point(struct point *, int, int);
static int post(int, int);
static int pushsnake(void);
static void setup(void);
static void snap(void);
static void snrand(struct point *);
static void spacewarp(int);
static void stop(int) __dead;
static int stretch(const struct point *);
static void surround(struct point *);
static void suspend(void);
static void win(const struct point *);
static void winnings(int);

int
main(int argc, char **argv)
{
	int     ch, i;
	time_t tv;

	/* Open score files then revoke setgid privileges */
	rawscores = open(_PATH_RAWSCORES, O_RDWR|O_CREAT, 0664);
	if (rawscores < 0) {
		warn("open %s", _PATH_RAWSCORES);
		sleep(2);
	} else if (rawscores < 3)
		exit(1);
	logfile = fopen(_PATH_LOGFILE, "a");
	if (logfile == NULL) {
		warn("fopen %s", _PATH_LOGFILE);
		sleep(2);
	}
	setgid(getgid());

	(void) time(&tv);

	while ((ch = getopt(argc, argv, "l:w:t")) != -1)
		switch ((char) ch) {
#ifdef DEBUG
		case 'd':
			tv = atol(optarg);
			break;
#endif
		case 'w':	/* width */
			ccnt = atoi(optarg);
			break;
		case 'l':	/* length */
			lcnt = atoi(optarg);
			break;
		case 't':
			fast = 0;
			break;
		case '?':
		default:
#ifdef DEBUG
			fprintf(stderr,
			    "usage: %s [-d seed] [-w width] [-l length] [-t]\n",
			    getprogname());
#else
			fprintf(stderr,
			    "usage: %s [-w width] [-l length] [-t]\n",
			    getprogname());
#endif
			exit(1);
		}

	srandom((int) tv);

	penalty = loot = 0;
	if (!initscr())
		errx(0, "couldn't initialize screen");;
	cbreak();
	noecho();
#ifdef KEY_LEFT
	keypad(stdscr, TRUE);
#endif
	if (!lcnt || lcnt > LINES - 2)
		lcnt = LINES - 2;
	if (!ccnt || ccnt > COLS - 2)
		ccnt = COLS - 2;

	i = MIN(lcnt, ccnt);
	if (i < 4) {
		endwin();
		errx(1, "screen too small for a fair game.");
	}
	/*
	 * chunk is the amount of money the user gets for each $.
	 * The formula below tries to be fair for various screen sizes.
	 * We only pay attention to the smaller of the 2 edges, since
	 * that seems to be the bottleneck.
	 * This formula is a hyperbola which includes the following points:
	 *	(24, $25)	(original scoring algorithm)
	 *	(12, $40)	(experimentally derived by the "feel")
	 *	(48, $15)	(a guess)
	 * This will give a 4x4 screen $99/shot.  We don't allow anything
	 * smaller than 4x4 because there is a 3x3 game where you can win
	 * an infinite amount of money.
	 */
	if (i < 12)
		i = 12;		/* otherwise it isn't fair */
	/*
	 * Compensate for border.  This really changes the game since
	 * the screen is two squares smaller but we want the default
	 * to be $25, and the high scores on small screens were a bit
	 * much anyway.
	 */
	i += 2;
	chunk = (675.0 / (i + 6)) + 2.5;	/* min screen edge */

	signal(SIGINT, stop);

	snrand(&finish);
	snrand(&you);
	snrand(&money);
	snrand(&snake[0]);

	for (i = 1; i < 6; i++)
		chase(&snake[i], &snake[i - 1]);
	setup();
	mainloop();
	/* NOTREACHED */
	return (0);
}

static struct point *
point(struct point *ps, int x, int y)
{
	ps->col = x;
	ps->line = y;
	return (ps);
}

/* Main command loop */
static void
mainloop(void)
{
	int     k;
	int     repeat = 1;
	int	lastc = 0;

	for (;;) {
		int     c;

		/* Highlight you, not left & above */
		move(you.line + 1, you.col + 1);
		refresh();
		if (((c = getch()) <= '9') && (c >= '0')) {
			repeat = c - '0';
			while (((c = getch()) <= '9') && (c >= '0'))
				repeat = 10 * repeat + (c - '0');
		} else {
			if (c != '.')
				repeat = 1;
		}
		if (c == '.') {
			c = lastc;
		}
		if (!fast)
			flushi();
		lastc = c;
		switch (c) {
		case CTRL('z'):
			suspend();
			continue;
		case EOT:
		case 'x':
		case 0177:	/* del or end of file */
			endwin();
			length(moves);
			logit("quit");
			exit(0);
		case CTRL('l'):
			setup();
			winnings(cashvalue);
			continue;
		case 'p':
		case 'd':
			snap();
			continue;
		case 'w':
			spacewarp(0);
			continue;
		case 'A':
			repeat = you.col;
			c = 'h';
			break;
		case 'H':
		case 'S':
			repeat = you.col - money.col;
			c = 'h';
			break;
		case 'T':
			repeat = you.line;
			c = 'k';
			break;
		case 'K':
		case 'E':
			repeat = you.line - money.line;
			c = 'k';
			break;
		case 'P':
			repeat = ccnt - 1 - you.col;
			c = 'l';
			break;
		case 'L':
		case 'F':
			repeat = money.col - you.col;
			c = 'l';
			break;
		case 'B':
			repeat = lcnt - 1 - you.line;
			c = 'j';
			break;
		case 'J':
		case 'C':
			repeat = money.line - you.line;
			c = 'j';
			break;
		}
		for (k = 1; k <= repeat; k++) {
			moves++;
			switch (c) {
			case 's':
			case 'h':
#ifdef KEY_LEFT
			case KEY_LEFT:
#endif
			case '\b':
				if (you.col > 0) {
					if ((fast) || (k == 1))
						pchar(&you, ' ');
					you.col--;
					if ((fast) || (k == repeat) ||
					    (you.col == 0))
						pchar(&you, ME);
				}
				break;
			case 'f':
			case 'l':
#ifdef KEY_RIGHT
			case KEY_RIGHT:
#endif
			case ' ':
				if (you.col < ccnt - 1) {
					if ((fast) || (k == 1))
						pchar(&you, ' ');
					you.col++;
					if ((fast) || (k == repeat) ||
					    (you.col == ccnt - 1))
						pchar(&you, ME);
				}
				break;
			case CTRL('p'):
			case 'e':
			case 'k':
#ifdef KEY_UP
			case KEY_UP:
#endif
			case 'i':
				if (you.line > 0) {
					if ((fast) || (k == 1))
						pchar(&you, ' ');
					you.line--;
					if ((fast) || (k == repeat) ||
					    (you.line == 0))
						pchar(&you, ME);
				}
				break;
			case CTRL('n'):
			case 'c':
			case 'j':
#ifdef KEY_DOWN
			case KEY_DOWN:
#endif
			case LF:
			case 'm':
				if (you.line + 1 < lcnt) {
					if ((fast) || (k == 1))
						pchar(&you, ' ');
					you.line++;
					if ((fast) || (k == repeat) ||
					    (you.line == lcnt - 1))
						pchar(&you, ME);
				}
				break;
			}

			if (same(&you, &money)) {
				loot += 25;
				if (k < repeat)
					pchar(&you, ' ');
				do {
					snrand(&money);
				} while ((money.col == finish.col &&
					money.line == finish.line) ||
				    (money.col < 5 && money.line == 0) ||
				    (money.col == you.col &&
					money.line == you.line));
				pchar(&money, TREASURE);
				winnings(cashvalue);
				continue;
			}
			if (same(&you, &finish)) {
				win(&finish);
				flushi();
				endwin();
				printf("You have won with $%d.\n", cashvalue);
				fflush(stdout);
				logit("won");
				post(cashvalue, 1);
				length(moves);
				exit(0);
			}
			if (pushsnake())
				break;
		}
	}
}

/*
 * setup the board
 */
static void
setup(void)
{
	int     i;

	erase();
	pchar(&you, ME);
	pchar(&finish, GOAL);
	pchar(&money, TREASURE);
	for (i = 1; i < 6; i++) {
		pchar(&snake[i], SNAKETAIL);
	}
	pchar(&snake[0], SNAKEHEAD);
	drawbox();
	refresh();
}

static void
drawbox(void)
{
	int i;

	for (i = 1; i <= ccnt; i++) {
		mvaddch(0, i, '-');
		mvaddch(lcnt + 1, i, '-');
	}
	for (i = 0; i <= lcnt + 1; i++) {
		mvaddch(i, 0, '|');
		mvaddch(i, ccnt + 1, '|');
	}
}

static void
snrand(struct point *sp)
{
	struct point p;
	int i;

	for (;;) {
		p.col = random() % ccnt;
		p.line = random() % lcnt;

		/* make sure it's not on top of something else */
		if (p.line == 0 && p.col < 5)
			continue;
		if (same(&p, &you))
			continue;
		if (same(&p, &money))
			continue;
		if (same(&p, &finish))
			continue;
		for (i = 0; i < 6; i++)
			if (same(&p, &snake[i]))
				break;
		if (i < 6)
			continue;
		break;
	}
	*sp = p;
}

static int
post(int iscore, int flag)
{
	short   score = iscore;
	short   uid;
	short   oldbest = 0;
	short   allbwho = 0, allbscore = 0;
	struct passwd *p;

	/* I want to printf() the scores for terms that clear on cook(),
	 * but this routine also gets called with flag == 0 to see if
	 * the snake should wink.  If (flag) then we're at game end and
	 * can printf.
	 */
	/*
	 * Neg uid, 0, and 1 cannot have scores recorded.
	 */
	if ((uid = getuid()) <= 1) {
		if (flag)
			printf("No saved scores for uid %d.\n", uid);
		return (1);
	}
	if (rawscores < 0) {
		/* Error reported earlier */
		return (1);
	}
	/* Figure out what happened in the past */
	read(rawscores, &allbscore, sizeof(short));
	read(rawscores, &allbwho, sizeof(short));
	lseek(rawscores, uid * sizeof(short), SEEK_SET);
	read(rawscores, &oldbest, sizeof(short));
	if (!flag) {
		lseek(rawscores, 0, SEEK_SET);
		return (score > oldbest ? 1 : 0);
	}

	/* Update this jokers best */
	if (score > oldbest) {
		lseek(rawscores, uid * sizeof(short), SEEK_SET);
		write(rawscores, &score, sizeof(short));
		printf("You bettered your previous best of $%d\n", oldbest);
	} else
		printf("Your best to date is $%d\n", oldbest);

	/* See if we have a new champ */
	p = getpwuid(allbwho);
	if (score > allbscore) {
		lseek(rawscores, 0, SEEK_SET);
		write(rawscores, &score, sizeof(short));
		write(rawscores, &uid, sizeof(short));
		if (allbwho) {
			if (p)
				printf("You beat %s's old record of $%d!\n",
				       p->pw_name, allbscore);
			else
				printf("You beat (%d)'s old record of $%d!\n",
				       (int)allbwho, allbscore);
		}
		else
			printf("You set a new record!\n");
	} else if (p)
		printf("The highest is %s with $%d\n", p->pw_name, allbscore);
	else
		printf("The highest is (%d) with $%d\n", (int)allbwho,
		    allbscore);
	lseek(rawscores, 0, SEEK_SET);
	return (1);
}

/*
 * Flush typeahead to keep from buffering a bunch of chars and then
 * overshooting.  This loses horribly at 9600 baud, but works nicely
 * if the terminal gets behind.
 */
static void
flushi(void)
{
	tcflush(0, TCIFLUSH);
}

static const int mx[8] = {
	0, 1, 1, 1, 0, -1, -1, -1
};
static const int my[8] = {
	-1, -1, 0, 1, 1, 1, 0, -1
};
static const float absv[8] = {
	1, 1.4, 1, 1.4, 1, 1.4, 1, 1.4
};
static int oldw = 0;

static void
chase(struct point *np, struct point *sp)
{
	/* this algorithm has bugs; otherwise the snake would get too good */
	struct point d;
	int     w, i, wt[8];
	double  v1, v2, vp, max;
	point(&d, you.col - sp->col, you.line - sp->line);
	v1 = sqrt((double) (d.col * d.col + d.line * d.line));
	w = 0;
	max = 0;
	for (i = 0; i < 8; i++) {
		vp = d.col * mx[i] + d.line * my[i];
		v2 = absv[i];
		if (v1 > 0)
			vp = ((double) vp) / (v1 * v2);
		else
			vp = 1.0;
		if (vp > max) {
			max = vp;
			w = i;
		}
	}
	for (i = 0; i < 8; i++) {
		point(&d, sp->col + mx[i], sp->line + my[i]);
		wt[i] = 0;
		if (d.col < 0 || d.col >= ccnt || d.line < 0 || d.line >= lcnt)
			continue;
		/*
		 * Change to allow snake to eat you if you're on the money,
		 * otherwise, you can just crouch there until the snake goes
		 * away.  Not positive it's right.
		 *
		 * if (d.line == 0 && d.col < 5) continue;
		 */
		if (same(&d, &money))
			continue;
		if (same(&d, &finish))
			continue;
		wt[i] = i == w ? loot / 10 : 1;
		if (i == oldw)
			wt[i] += loot / 20;
	}
	for (w = i = 0; i < 8; i++)
		w += wt[i];
	vp = ((random() >> 6) & 01777) % w;
	for (i = 0; i < 8; i++)
		if (vp < wt[i])
			break;
		else
			vp -= wt[i];
	if (i == 8) {
		printw("failure\n");
		i = 0;
		while (wt[i] == 0)
			i++;
	}
	oldw = w = i;
	point(np, sp->col + mx[w], sp->line + my[w]);
}

static void
spacewarp(int w)
{
	struct point p;
	int     j;
	const char   *str;

	snrand(&you);
	point(&p, COLS / 2 - 8, LINES / 2 - 1);
	if (p.col < 0)
		p.col = 0;
	if (p.line < 0)
		p.line = 0;
	if (w) {
		str = "BONUS!!!";
		loot = loot - penalty;
		penalty = 0;
	} else {
		str = "SPACE WARP!!!";
		penalty += loot / PENALTY;
	}
	for (j = 0; j < 3; j++) {
		erase();
		refresh();
		delay(5);
		mvaddstr(p.line + 1, p.col + 1, str);
		refresh();
		delay(10);
	}
	setup();
	winnings(cashvalue);
}

static void
snap(void)
{
#if 0 /* This code doesn't really make sense.  */
	struct point p;

	if (you.line < 3) {
		mvaddch(1, you.col + 1, '-');
	}
	if (you.line > lcnt - 4) {
		mvaddch(lcnt, you.col + 1, '_');
	}
	if (you.col < 10) {
		mvaddch(you.line + 1, 1, '(');
	}
	if (you.col > ccnt - 10) {
		mvaddch(you.line + 1, ccnt, ')');
	}
#endif
	if (!stretch(&money))
		if (!stretch(&finish)) {
			pchar(&you, '?');
			refresh();
			delay(10);
			pchar(&you, ME);
		}
#if 0
	if (you.line < 3) {
		point(&p, you.col, 0);
		chk(&p);
	}
	if (you.line > lcnt - 4) {
		point(&p, you.col, lcnt - 1);
		chk(&p);
	}
	if (you.col < 10) {
		point(&p, 0, you.line);
		chk(&p);
	}
	if (you.col > ccnt - 10) {
		point(&p, ccnt - 1, you.line);
		chk(&p);
	}
#endif
	refresh();
}

static int
stretch(const struct point *ps)
{
	struct point p;

	point(&p, you.col, you.line);
	if ((abs(ps->col - you.col) < (ccnt / 12)) && (you.line != ps->line)) {
		if (you.line < ps->line) {
			for (p.line = you.line + 1; p.line <= ps->line; p.line++)
				pchar(&p, 'v');
			refresh();
			delay(10);
			for (; p.line > you.line; p.line--)
				chk(&p);
		} else {
			for (p.line = you.line - 1; p.line >= ps->line; p.line--)
				pchar(&p, '^');
			refresh();
			delay(10);
			for (; p.line < you.line; p.line++)
				chk(&p);
		}
		return (1);
	} else
		if ((abs(ps->line - you.line) < (lcnt/7))
		    && (you.col != ps->col)) {
			p.line = you.line;
			if (you.col < ps->col) {
				for (p.col = you.col + 1; p.col <= ps->col; p.col++)
					pchar(&p, '>');
				refresh();
				delay(10);
				for (; p.col > you.col; p.col--)
					chk(&p);
			} else {
				for (p.col = you.col - 1; p.col >= ps->col; p.col--)
					pchar(&p, '<');
				refresh();
				delay(10);
				for (; p.col < you.col; p.col++)
					chk(&p);
			}
			return (1);
		}
	return (0);
}

static void
surround(struct point *ps)
{
	int     j;

	if (ps->col == 0)
		ps->col++;
	if (ps->line == 0)
		ps->line++;
	if (ps->line == LINES - 1)
		ps->line--;
	if (ps->col == COLS - 1)
		ps->col--;
	mvaddstr(ps->line, ps->col, "/*\\");
	mvaddstr(ps->line + 1, ps->col, "* *");
	mvaddstr(ps->line + 2, ps->col, "\\*/");
	for (j = 0; j < 20; j++) {
		pchar(ps, '@');
		refresh();
		delay(1);
		pchar(ps, ' ');
		refresh();
		delay(1);
	}
	if (post(cashvalue, 0)) {
		mvaddstr(ps->line, ps->col, "   ");
		mvaddstr(ps->line + 1, ps->col, "o.o");
		mvaddstr(ps->line + 2, ps->col, "\\_/");
		refresh();
		delay(6);
		mvaddstr(ps->line, ps->col, "   ");
		mvaddstr(ps->line + 1, ps->col, "o.-");
		mvaddstr(ps->line + 2, ps->col, "\\_/");
		refresh();
		delay(6);
	}
	mvaddstr(ps->line, ps->col, "   ");
	mvaddstr(ps->line + 1, ps->col, "o.o");
	mvaddstr(ps->line + 2, ps->col, "\\_/");
	refresh();
	delay(6);
}

static void
win(const struct point *ps)
{
	struct point x;
	int     j, k;
	int     boxsize;	/* actually diameter of box, not radius */

	boxsize = fast ? 10 : 4;
	point(&x, ps->col, ps->line);
	for (j = 1; j < boxsize; j++) {
		for (k = 0; k < j; k++) {
			pchar(&x, '#');
			x.line--;
		}
		for (k = 0; k < j; k++) {
			pchar(&x, '#');
			x.col++;
		}
		j++;
		for (k = 0; k < j; k++) {
			pchar(&x, '#');
			x.line++;
		}
		for (k = 0; k < j; k++) {
			pchar(&x, '#');
			x.col--;
		}
		refresh();
		delay(1);
	}
}

static int
pushsnake(void)
{
	int     i, bonus;
	int     issame = 0;
	struct point tmp;

	/*
	 * My manual says times doesn't return a value.  Furthermore, the
	 * snake should get his turn every time no matter if the user is
	 * on a fast terminal with typematic keys or not.
	 * So I have taken the call to times out.
	 */
	for (i = 4; i >= 0; i--)
		if (same(&snake[i], &snake[5]))
			issame++;
	if (!issame)
		pchar(&snake[5], ' ');
	/* Need the following to catch you if you step on the snake's tail */
	tmp.col = snake[5].col;
	tmp.line = snake[5].line;
	for (i = 4; i >= 0; i--)
		snake[i + 1] = snake[i];
	chase(&snake[0], &snake[1]);
	pchar(&snake[1], SNAKETAIL);
	pchar(&snake[0], SNAKEHEAD);
	for (i = 0; i < 6; i++) {
		if (same(&snake[i], &you) || same(&tmp, &you)) {
			surround(&you);
			i = (cashvalue) % 10;
			bonus = ((random() >> 8) & 0377) % 10;
			mvprintw(lcnt + 1, 0, "%d\n", bonus);
			refresh();
			delay(30);
			if (bonus == i) {
				spacewarp(1);
				logit("bonus");
				flushi();
				return (1);
			}
			flushi();
			endwin();
			if (loot >= penalty) {
				printf("\nYou and your $%d have been eaten\n",
				    cashvalue);
			} else {
				printf("\nThe snake ate you.  You owe $%d.\n",
				    -cashvalue);
			}
			logit("eaten");
			length(moves);
			exit(0);
		}
	}
	return (0);
}

static int
chk(const struct point *sp)
{
	int     j;

	if (same(sp, &money)) {
		pchar(sp, TREASURE);
		return (2);
	}
	if (same(sp, &finish)) {
		pchar(sp, GOAL);
		return (3);
	}
	if (same(sp, &snake[0])) {
		pchar(sp, SNAKEHEAD);
		return (4);
	}
	for (j = 1; j < 6; j++) {
		if (same(sp, &snake[j])) {
			pchar(sp, SNAKETAIL);
			return (4);
		}
	}
	if ((sp->col < 4) && (sp->line == 0)) {
		winnings(cashvalue);
		if ((you.line == 0) && (you.col < 4))
			pchar(&you, ME);
		return (5);
	}
	if (same(sp, &you)) {
		pchar(sp, ME);
		return (1);
	}
	pchar(sp, ' ');
	return (0);
}

static void
winnings(int won)
{
	if (won > 0) {
		mvprintw(1, 1, "$%d", won);
	}
}

static void
stop(int dummy __unused)
{
	signal(SIGINT, SIG_IGN);
	endwin();
	length(moves);
	exit(0);
}

static void
suspend(void)
{
	endwin();
	kill(getpid(), SIGTSTP);
	refresh();
	winnings(cashvalue);
}

static void
length(int num)
{
	printf("You made %d moves.\n", num);
}

static void
logit(const char *msg)
{
	time_t  t;

	if (logfile != NULL) {
		time(&t);
		fprintf(logfile, "%s $%d %dx%d %s %s",
		    getlogin(), cashvalue, lcnt, ccnt, msg, ctime(&t));
		fflush(logfile);
	}
}
