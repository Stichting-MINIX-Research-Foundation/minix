/* life - Conway's game of life		Author: Jim King */

/* clife.c - curses life simulator.  Translated from Pascal to C implementing
 *           curses Oct 1988 by pulsar@lsrhs, not jek5036@ritvax.isc.rit.edu
 *           life needs about 18kb stack space on MINIX.
 *
 * Flags:	-d  draw your own screen using arrows and space bar
 *		-p  print statistics on the bottom line during the game
 */

#include <sys/types.h>
#include <signal.h>
#include <time.h>
#include <curses.h>
#include <stdlib.h>
#include <unistd.h>
#include <stdio.h>

#if __minix_vmd		/* Use a more random rand(). */
#define srand(seed)	srandom(seed)
#define rand()		random()
#endif

/* A value of -1 will make it go forever */
/* A value of 0 will make it exit immediately */
#define	REPSTOP		-1	/* number of repetitions before stop */

int present[23][80];		/* screen 1 cycle ago */
int past[23][80];		/* screen this cycle */
int total;			/* total # of changes */
int icnt;			/* counter to check for repetition */
int maxrow = 22;		/* some defines to represent the screen */
int maxcol = 79;
int minrow = 0;
int mincol = 0; 
int pri = 0;			/* flag for printing stats on bottom line */
int draw = 0;			/* flag for drawing your own screen */
int i, j, k;			/* loop counters */
int cycle;			/* current cycle # */
int changes;			/* # of changes this cycle (live + die) */
int die;			/* number of deaths this cycle */
int live;			/* number of births this cycle */

WINDOW *mns;			/* Main Screen */
WINDOW *info;			/* Bottom line */

_PROTOTYPE(void cleanup, (int s));
_PROTOTYPE(void initialize, (void));
_PROTOTYPE(void makscr, (void));
_PROTOTYPE(void update, (void));
_PROTOTYPE(void print, (void));
_PROTOTYPE(int main, (int ac, char *av[]));

/* Cleanup - cleanup then exit */
void cleanup(s)
int s;
{
  move(23, 0);			/* go to bottom of screen */
  refresh();			/* update cursor */

  endwin();			/* shutdown curses */
  exit(1);			/* exit */
}

/* Initialize - init windows, variables, and signals */

void initialize()
{
  srand(getpid());		/* init random seed */
  initscr();			/* init curses */
  noecho();
  curs_set(0);
  signal(SIGINT, cleanup);	/* catch ^C */
  mns = newwin(maxrow, maxcol, 0, 0);	/* new window */
  scrollok(mns, FALSE);
  info = newwin(1, 80, 23, 0);
  scrollok(info, FALSE);
  wclear(mns);
  wclear(info);
  wmove(info, 0, 0);
  wrefresh(info);
  if (!draw) {			/* if no draw, make random pattern */
	for (j = 0; j < maxrow; j++) {
		for (k = 0; k < maxcol; k++) {
			present[j][k] = rand() % 2;
			if (present[j][k] == 1) changes++, live++;
		}
	}
  }
}

/* Makscr - make your own screen using arrow keys and space bar */
void makscr()
{
  int curx, cury;		/* current point on screen */
  char c;			/* input char */

  wclear(info);
  wmove(info, 0, 0);
  wprintw(info, "Use arrow keys to move, space to place / erase, ^D to start", NULL);
  wrefresh(info);
  curx = cury = 1;
  wmove(mns, cury - 1, curx - 1);
  wrefresh(mns);
  noecho();
  for (;;) {
	c = wgetch(mns);
	if (c == '\004')
		break;
	else if (c == ' ') {
		if (present[cury][curx]) {
			--present[cury][curx];
			changes++;
			die++;
			mvwaddch(mns, cury, curx, ' ');
		} else {
			++present[cury][curx];
			changes++;
			live++;
			mvwaddch(mns, cury, curx, '*');
		}
	} else if (c == '\033') {
		wgetch(mns);
		switch (wgetch(mns)) {
		    case 'A':	--cury;	break;
		    case 'B':	++cury;	break;
		    case 'C':	++curx;	break;
		    case 'D':	--curx;	break;
		    default:	break;
		}
	}
	if (cury > maxrow) cury = minrow;
	if (cury < minrow) cury = maxrow;
	if (curx > maxcol) curx = mincol;
	if (curx < mincol) curx = maxcol;
	wmove(mns, cury, curx);
	wrefresh(mns);
  }
  wclear(info);
}

/* Update rules:  2 or 3 adjacent alive --- stay alive
 *                3 adjacent alive -- dead to live
 *                all else die or stay dead
 */
void update()
{				/* Does all mathmatical calculations */
  int howmany, w, x, y, z;
  changes = die = live = 0;
  for (j = 0; j < maxrow; j++) {
	for (k = 0; k < maxcol; k++) {
		w = j - 1;
		x = j + 1;
		y = k - 1;
		z = k + 1;

		howmany = (past[w][y] + past[w][k] + past[w][z] +
			   past[j][y] + past[j][z] + past[x][y] +
			   past[x][k] + past[x][z]);

		switch (howmany) {
		    case 0:
		    case 1:
		    case 4:
		    case 5:
		    case 6:
		    case 7:
		    case 8:
			present[j][k] = 0;
			if (past[j][k]) changes++, die++;
			break;
		    case 3:
			present[j][k] = 1;
			if (!past[j][k]) changes++, live++;
			break;
		    default:	break;
		}
	}
  }
  if (live == die)
	++icnt;
  else
	icnt = 0;

  if (icnt == REPSTOP) cleanup(0);
}

/* Print - updates the screen according to changes from past to present */
void print()
{	
/* Updates the screen, greatly improved using curses */
  if (pri) {
	wmove(info, 0, 0);
	total += changes;
	cycle++;
	wprintw(info, "Cycle %5d | %5d changes: %5d died + %5d born = %5u total changes", (char *) cycle, changes, die, live, total);
	wclrtoeol(info);
  }
  for (j = 1; j < maxrow; j++) {
	for (k = 1; k < maxcol; k++) {
		if (present[j][k] != past[j][k] && present[j][k] == 1) {
			wmove(mns, j, k);
			wprintw(mns, "*", NULL);
		} else if (present[j][k] != past[j][k] && present[j][k] == 0) {
			wmove(mns, j, k);
			wprintw(mns, " ", NULL);
		}
	}
  }
  if (pri) wrefresh(info);
  wrefresh(mns);
}

/* Main - main procedure */
int main(ac, av)
int ac;
char *av[];
{
  if (ac > 1) {
	for (j = 1; j < ac; j++) {
		switch (av[j][1]) {
		    case 'd':	++draw;	break;
		    case 'p':	++pri;	break;
		    default:
			fprintf(stderr, "%s: usage: %s [-d] [-p]\n", av[0], av[0]);
			exit(1);
		}
	}
  }

  initialize();
  if (draw) makscr();

  for (;;) {
	print();
	for (j = 0; j < maxrow; j++) {
		for (k = 0; k < maxcol; k++) past[j][k] = present[j][k];
	}
	update();
  }
}
