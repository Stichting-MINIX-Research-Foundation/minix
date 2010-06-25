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
CPPFLAGS+= -w -DCRUNCH \
	-DNO_MKEXRC -DNO_CURSORSHAPE -DNO_CHARATTR -DNO_SHOWMODE \
	-DNO_MODELINE -DNO_OPTCOLS -DNO_DIGRAPH -DNO_EXTENSIONS \
	-DNO_ERRLIST -DNO_FKEY -DNO_VISIBLE -DNO_COLOR -DNO_POPUP

###########################################################################
###     The rest of this Makefile contains no user-serviceable parts    ###
###########################################################################

SRCS.elvis= blk.c cmd1.c cmd2.c ctype.c curses.c cut.c ex.c input.c \
   main.c misc.c modify.c move1.c move2.c move3.c move4.c move5.c \
   opts.c recycle.c redraw.c regexp.c regsub.c system.c tio.c tmp.c \
   unix.c vars.c vcmd.c vi.c

BINDIR= /usr/bin
.if !exists(${BINDIR}/vi)
LINKS+= ${BINDIR}/elvis ${BINDIR}/vi
.endif

MAN.elvis=
MAN.ctags=
MAN.ref=
MAN.elvrec=
MAN.fmt=
MAN.elvprsv=

.include <bsd.prog.mk>
