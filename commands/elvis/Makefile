# Makefile for elvis
#
# Several groups of Makefile settings are included below.  Choose *ONE* group
# of settings for your particular system, and leave the others commented out.
# The meanings of these settings are:
#	EXTRA	version-specific object files used in elvis
#	CC	the C compiler command, possibly with "memory model" flags
#	CFLAGS	compiler flags used to select compile-time options
#	PROGS	the list of all programs
#	SORT	if the "tags" file must be sorted, then SORT=-DSORT

PROGS=	elvis ctags ref elvrec fmt elvprsv

#---- These settings are recommended for Minix-PC ----
EXTRA=	tinytcap.o tinyprnt.o
CFLAGS=	-O -w -D_POSIX_SOURCE -D_MINIX -DCRUNCH \
	-DNO_MKEXRC -DNO_CURSORSHAPE -DNO_CHARATTR -DNO_SHOWMODE \
	-DNO_MODELINE -DNO_OPTCOLS -DNO_DIGRAPH -DNO_EXTENSIONS \
	-DNO_ERRLIST -DNO_FKEY -DNO_VISIBLE -DNO_COLOR -DNO_POPUP
LDFLAGS=-i

#---- These settings are recommended for Minix-ST ----
#EXTRA=
#CFLAGS=

###########################################################################
###     The rest of this Makefile contains no user-serviceable parts    ###
###########################################################################

OBJ=blk.o cmd1.o cmd2.o ctype.o curses.o cut.o ex.o input.o \
   main.o misc.o modify.o move1.o move2.o move3.o move4.o move5.o \
   opts.o recycle.o redraw.o regexp.o regsub.o system.o tio.o tmp.o \
   unix.o vars.o vcmd.o vi.o

all:	$(PROGS)

elvis:	$(OBJ)
	$(CC) $(LDFLAGS) -o elvis $(OBJ)
	install -S 18kw elvis

ctags:	ctags.c
	$(CC) $(CFLAGS) $(SORT) $(LDFLAGS) -o ctags ctags.c
	install -S 4kw $@

ref:	ref.c
	$(CC) $(CFLAGS) $(LDFLAGS) -o ref ref.c
	install -S 4kw $@

elvrec:	elvrec.c
	$(CC) $(CFLAGS) $(LDFLAGS) -o elvrec elvrec.c
	install -S 4kw $@

fmt:	fmt.c
	$(CC) $(CFLAGS) $(LDFLAGS) -o fmt fmt.c
	install -S 4kw $@

elvprsv:	elvprsv.c
	$(CC) $(CFLAGS) $(LDFLAGS) -o elvprsv elvprsv.c ctype.o
	install -S 4kw $@

install:	/usr/bin/elvis /usr/bin/ex /usr/bin/vi \
		/usr/bin/ctags /usr/bin/ref /usr/bin/fmt \
		/usr/bin/elvrec /usr/bin/elvprsv

/usr/bin/elvis:	elvis
	install -cs -o bin elvis $@

/usr/bin/ex /usr/bin/vi:	/usr/bin/elvis
	install -l /usr/bin/elvis $@

/usr/bin/ctags:	ctags
	install -cs -o bin ctags $@

/usr/bin/ref:	ref
	install -cs -o bin ref $@

/usr/bin/fmt:	fmt
	install -cs -o bin fmt $@

/usr/bin/elvrec:	elvrec
	install -cs -o root -m 4755 elvrec $@

/usr/bin/elvprsv:	elvprsv
	install -cs -o root -m 4755 elvprsv $@

# Dependencies
$(OBJ):	vi.h curses.h config.h regexp.h ctype.h

clean:	
	rm -f *.o ctags ref elvrec fmt elvprsv elvis
