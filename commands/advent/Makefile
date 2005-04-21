# Makefile for advent

# Where to put the adventure text files, and the binary executable.
# Need the trailing "/"s.
TEXTDIR	= /usr/lib/advent/
BINDIR	= /usr/bin

# Flags you may want to add to CFLAGS:
# -DHAS_STDC=0 or 1	We have Standard C.  Default=1 iff __STDC__ is nonzero.

CC	= cc
CFLAGS	= -D_POSIX_SOURCE
LDFLAGS	=  -i

OBJS	= advent.o database.o english.o initial.o itverb.o score.o\
	travel.o turn.o utility.o verb.o vocab.o

DAT	= advent1.dat advent2.dat advent3.dat advent4.dat
INSTDAT	= $(TEXTDIR)advent1.dat $(TEXTDIR)advent2.dat \
	  $(TEXTDIR)advent3.dat $(TEXTDIR)advent4.dat

all:	$(DAT) advent

install:	$(TEXTDIR) $(INSTDAT) $(BINDIR)/advent

$(TEXTDIR):
	install -d -o bin $(TEXTDIR)

$(TEXTDIR)advent1.dat:	advent1.dat
	install -c -o bin $? $@

$(TEXTDIR)advent2.dat:	advent2.dat
	install -c -o bin $? $@

$(TEXTDIR)advent3.dat:	advent3.dat
	install -c -o bin $? $@

$(TEXTDIR)advent4.dat:	advent4.dat
	install -c -o bin $? $@

$(BINDIR)/advent:	advent
	install -cs -o bin $? $@

advent:	$(OBJS)
	$(CC) $(LDFLAGS) -o advent $(OBJS)

setup: setup.c advent.h
	$(CC) $(CFLAGS) $(LDFLAGS) -o setup setup.c
 
advtext.h advent1.dat advent2.dat advent3.dat advent4.dat: \
		setup advent1.txt advent2.txt advent3.txt advent4.txt
	./setup

advent.o: advent.h advdec.h advent.c
	$(CC) -c $(CFLAGS) -DTEXTDIR='"$(TEXTDIR)"' advent.c

database.o: advent.h advdec.h advtext.h
travel.o: advent.h advdec.h advcave.h
initial.o english.o itverb.o score.o turn.o utility.o\
	verb.o vocab.o: advent.h advdec.h

clean:	
	@rm -f *.o *.BAK *.dat advtext.h core advent setup
