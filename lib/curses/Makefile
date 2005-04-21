# Makefile for lib/curses.

CFLAGS	= -O -D_MINIX -D_POSIX_SOURCE
CC1	= $(CC) $(CFLAGS) -c

LIBRARY	= ../libcurses.a
all:	$(LIBRARY)

OBJECTS	= \
	$(LIBRARY)(beep.o) \
	$(LIBRARY)(charpick.o) \
	$(LIBRARY)(curs_set.o) \
	$(LIBRARY)(cursesio.o) \
	$(LIBRARY)(endwin.o) \
	$(LIBRARY)(flash.o) \
	$(LIBRARY)(initscr.o) \
	$(LIBRARY)(longname.o) \
	$(LIBRARY)(move.o) \
	$(LIBRARY)(mvcursor.o) \
	$(LIBRARY)(newwin.o) \
	$(LIBRARY)(options.o) \
	$(LIBRARY)(overlay.o) \
	$(LIBRARY)(prntscan.o) \
	$(LIBRARY)(refresh.o) \
	$(LIBRARY)(scrreg.o) \
	$(LIBRARY)(setterm.o) \
	$(LIBRARY)(tabsize.o) \
	$(LIBRARY)(termmisc.o) \
	$(LIBRARY)(unctrl.o) \
	$(LIBRARY)(update.o) \
	$(LIBRARY)(waddch.o) \
	$(LIBRARY)(waddstr.o) \
	$(LIBRARY)(wbox.o) \
	$(LIBRARY)(wclear.o) \
	$(LIBRARY)(wclrtobot.o) \
	$(LIBRARY)(wclrtoeol.o) \
	$(LIBRARY)(wdelch.o) \
	$(LIBRARY)(wdeleteln.o) \
	$(LIBRARY)(werase.o) \
	$(LIBRARY)(wgetch.o) \
	$(LIBRARY)(wgetstr.o) \
	$(LIBRARY)(windel.o) \
	$(LIBRARY)(winmove.o) \
	$(LIBRARY)(winsch.o) \
	$(LIBRARY)(winscrol.o) \
	$(LIBRARY)(winsertln.o) \
	$(LIBRARY)(wintouch.o) \

$(LIBRARY):	$(OBJECTS)
	aal cr $@ *.o
	rm *.o

$(LIBRARY)(beep.o):	beep.c
	$(CC1) beep.c

$(LIBRARY)(charpick.o):	charpick.c
	$(CC1) charpick.c

$(LIBRARY)(curs_set.o):	curs_set.c
	$(CC1) curs_set.c

$(LIBRARY)(cursesio.o):	cursesio.c
	$(CC1) cursesio.c

$(LIBRARY)(endwin.o):	endwin.c
	$(CC1) endwin.c

$(LIBRARY)(flash.o):	flash.c
	$(CC1) flash.c

$(LIBRARY)(initscr.o):	initscr.c
	$(CC1) initscr.c

$(LIBRARY)(longname.o):	longname.c
	$(CC1) longname.c

$(LIBRARY)(move.o):	move.c
	$(CC1) move.c

$(LIBRARY)(mvcursor.o):	mvcursor.c
	$(CC1) mvcursor.c

$(LIBRARY)(newwin.o):	newwin.c
	$(CC1) newwin.c

$(LIBRARY)(options.o):	options.c
	$(CC1) options.c

$(LIBRARY)(overlay.o):	overlay.c
	$(CC1) overlay.c

$(LIBRARY)(prntscan.o):	prntscan.c
	$(CC1) prntscan.c

$(LIBRARY)(refresh.o):	refresh.c
	$(CC1) refresh.c

$(LIBRARY)(scrreg.o):	scrreg.c
	$(CC1) scrreg.c

$(LIBRARY)(setterm.o):	setterm.c
	$(CC1) setterm.c

$(LIBRARY)(tabsize.o):	tabsize.c
	$(CC1) tabsize.c

$(LIBRARY)(termmisc.o):	termmisc.c
	$(CC1) termmisc.c

$(LIBRARY)(unctrl.o):	unctrl.c
	$(CC1) unctrl.c

$(LIBRARY)(update.o):	update.c
	$(CC1) update.c

$(LIBRARY)(waddch.o):	waddch.c
	$(CC1) waddch.c

$(LIBRARY)(waddstr.o):	waddstr.c
	$(CC1) waddstr.c

$(LIBRARY)(wbox.o):	wbox.c
	$(CC1) wbox.c

$(LIBRARY)(wclear.o):	wclear.c
	$(CC1) wclear.c

$(LIBRARY)(wclrtobot.o):	wclrtobot.c
	$(CC1) wclrtobot.c

$(LIBRARY)(wclrtoeol.o):	wclrtoeol.c
	$(CC1) wclrtoeol.c

$(LIBRARY)(wdelch.o):	wdelch.c
	$(CC1) wdelch.c

$(LIBRARY)(wdeleteln.o):	wdeleteln.c
	$(CC1) wdeleteln.c

$(LIBRARY)(werase.o):	werase.c
	$(CC1) werase.c

$(LIBRARY)(wgetch.o):	wgetch.c
	$(CC1) wgetch.c

$(LIBRARY)(wgetstr.o):	wgetstr.c
	$(CC1) wgetstr.c

$(LIBRARY)(windel.o):	windel.c
	$(CC1) windel.c

$(LIBRARY)(winmove.o):	winmove.c
	$(CC1) winmove.c

$(LIBRARY)(winsch.o):	winsch.c
	$(CC1) winsch.c

$(LIBRARY)(winscrol.o):	winscrol.c
	$(CC1) winscrol.c

$(LIBRARY)(winsertln.o):	winsertln.c
	$(CC1) winsertln.c

$(LIBRARY)(wintouch.o):	wintouch.c
	$(CC1) wintouch.c
