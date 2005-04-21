/* screen.c Copyright Michael Temari 08/01/1996 All Rights Reserved */

#include <sys/types.h>
#include <sys/stat.h>
#include <stdio.h>
#include <signal.h>
#include <ctype.h>
#include <string.h>
#include <unistd.h>
#include <curses.h>

#include "screen.h"

_PROTOTYPE(void gotsig, (int sig));
_PROTOTYPE(static char *delword, (WINDOW *w));

struct {
	WINDOW *win;
	char erase;
	char kill;
	char werase;
} window[2];

static char line[80+1];

int ScreenDone = 0;

static WINDOW *dwin;

void gotsig(sig)
int sig;
{
   ScreenDone = 1;
   signal(sig, gotsig);
}

int ScreenInit()
{
int i;

   if(initscr() == (WINDOW *)NULL) {
   	fprintf(stderr, "talk: Could not initscr\n");
   	return(-1);
   }
   signal(SIGINT, gotsig);
   signal(SIGQUIT, gotsig);
   signal(SIGPIPE, gotsig);
   signal(SIGHUP, gotsig);
   clear();
   refresh();
   noecho();
   cbreak();

   /* local window */
   window[LOCALWIN].win = newwin(LINES / 2, COLS, 0, 0);
   scrollok(window[LOCALWIN].win, TRUE);
   wclear(window[LOCALWIN].win);

   /* divider between windows */
   dwin = newwin(1, COLS, LINES / 2, 0);
   i = COLS;
   while(i-- > 0)
   	waddch(dwin, '-');
   wrefresh(dwin);

   /* remote window */
   window[REMOTEWIN].win = newwin(LINES - (LINES / 2) - 1, COLS, LINES / 2 + 1, 0);
   scrollok(window[REMOTEWIN].win, TRUE);
   wclear(window[REMOTEWIN].win);

   return(0);
}

void ScreenMsg(msg)
char *msg;
{
WINDOW *w;

   w =window[LOCALWIN].win;

   wmove(w, 0, 0);

   if(*msg != '\0') {
	wprintw(w, "[%s]", msg);
	wclrtoeol(w);
   } else
   	werase(w);

   wrefresh(w);
}

void ScreenWho(user, host)
char *user;
char *host;
{
   if(*host != '\0') {
   	wmove(dwin, 0, (COLS - (1 + strlen(user) + 1 + strlen(host) + 1)) / 2);
   	wprintw(dwin, " %s@%s ", user, host);
   } else {
   	wmove(dwin, 0, (COLS - (1 + strlen(user) + 1)) / 2);
   	wprintw(dwin, " %s ", user);
   }
   wrefresh(dwin);
}

void ScreenEdit(lcc, rcc)
char lcc[];
char rcc[];
{
   window[LOCALWIN].erase   = lcc[0];
   window[LOCALWIN].kill    = lcc[1];
   window[LOCALWIN].werase  = lcc[2];
   window[REMOTEWIN].erase  = rcc[0];
   window[REMOTEWIN].kill   = rcc[1];
   window[REMOTEWIN].werase = rcc[2];
}

void ScreenPut(data, len, win)
char *data;
int len;
int win;
{
WINDOW *w;
unsigned char ch;
int r, c;

   w = window[win].win;

   while(len-- > 0) {
   	ch = *data++;
   	/* new line CR, NL */
   	if(ch == '\r' || ch == '\n') {
		waddch(w, '\n');
	} else
	/* erase a character, BS, DEL  */
	if(ch == 0x08 || ch == 0x7f || ch == window[win].erase) {
		getyx(w, r, c);
		if(c > 0)
			c--;
		wmove(w, r, c);
		waddch(w, ' ');
		wmove(w, r, c);
	} else
	/* erase line CTL-U */
	if(ch == 0x15 || ch == window[win].kill) {
		getyx(w, r, c);
		wmove(w, r, 0);
		wclrtoeol(w);
	} else
	/* refresh CTL-L */
	if(ch == 0x0c) {
		if(win == LOCALWIN) {
			touchwin(w);
			wrefresh(w);
			touchwin(window[REMOTEWIN].win);
			wrefresh(window[REMOTEWIN].win);
		}
	} else
	/* bell CTL-G */
	if(ch == 0x07) {
		putchar(ch);
	}
	else
	/* erase last word CTL-W */
	if(ch == 0x17 || ch == window[win].werase) {
		(void) delword(w);
	} else {
		getyx(w, r, c);
		if(1 || isprint(ch)) {
			if(ch != ' ' && c == (COLS - 1))
				wprintw(w, "\n%s", delword(w));
			waddch(w, ch);
		}
	}
   }
   wrefresh(w);
}

static char *delword(w)
WINDOW *w;
{
int r, c;
int i = 0;
char ch;
char *p = &line[80];

   *p-- = '\0';
   getyx(w, r, c);
   if(c == 0) return;
   while(c >= 0) {
   	c--;
   	ch = mvwinch(w, r, c);
   	if(ch == ' ') break;
   	*p-- = ch;
   	i = 1;
   	waddch(w, ' ');
   }
   c += i;
   wmove(w, r, c);
   return(++p);
}

void ScreenEnd()
{
   move(LINES - 1, 0);
   refresh();
   endwin();
}
