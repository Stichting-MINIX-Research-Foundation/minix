# Makefile for advent

PROG=	advent
SRCS=	advent.c database.c english.c initial.c itverb.c score.c \
	travel.c turn.c utility.c verb.c vocab.c

# Where to put the adventure text files
# Need the trailing "/" on TEXTDIR
TEXTDIR=	/usr/lib/advent/
FILESDIR=	${TEXTDIR}
DATFILES=	advent1.dat advent2.dat advent3.dat advent4.dat
FILES=		${DATFILES}
MAN=

database.o: advtext.h

setup: setup.c advent.h
	${CC} ${CPPFLAGS} ${LDFLAGS} -o setup setup.c

advtext.h advent1.dat advent2.dat advent3.dat advent4.dat: \
		setup advent1.txt advent2.txt advent3.txt advent4.txt
	./setup

CPPFLAGS.advent.c= -DTEXTDIR='"${TEXTDIR}"'

CLEANFILES+=	${DATFILES} advtext.h setup

.include <minix.prog.mk>
