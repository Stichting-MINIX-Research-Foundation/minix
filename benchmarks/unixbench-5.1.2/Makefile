##############################################################################
#	UnixBench v5.1.1
#  Based on The BYTE UNIX Benchmarks - Release 3
#          Module: Makefile   SID: 3.9 5/15/91 19:30:15
#
##############################################################################
# Bug reports, patches, comments, suggestions should be sent to:
#	David C Niemi <niemi@tux.org>
#
# Original Contacts at Byte Magazine:
#	Ben Smith or Tom Yager at BYTE Magazine
#	bensmith@bytepb.byte.com    tyager@bytepb.byte.com
#
##############################################################################
#  Modification Log: 7/28/89 cleaned out workload files
#                    4/17/90 added routines for installing from shar mess
#                    7/23/90 added compile for dhrystone version 2.1
#                          (this is not part of Run file. still use old)
#                          removed HZ from everything but dhry.
#                          HZ is read from the environment, if not
#                          there, you must define it in this file
#                    10/30/90 moved new dhrystone into standard set
#                          new pgms (dhry included) run for a specified
#                          time rather than specified number of loops
#                    4/5/91 cleaned out files not needed for
#                          release 3 -- added release 3 files -ben
#                    10/22/97 added compiler options for strict ANSI C
#                          checking for gcc and DEC's cc on
#                          Digital Unix 4.x (kahn@zk3.dec.com)
#                    09/26/07 changes for UnixBench 5.0
#                    09/30/07 adding ubgears, GRAPHIC_TESTS switch
#                    10/14/07 adding large.txt
##############################################################################

##############################################################################
# CONFIGURATION
##############################################################################

SHELL = /bin/sh

# GRAPHICS TESTS: Uncomment the definition of "GRAPHIC_TESTS" to enable
# the building of the graphics benchmarks.  This will require the
# X11 libraries on your system.
#
# Comment the line out to disable these tests.
# GRAPHIC_TESTS = defined

# Set "GL_LIBS" to the libraries needed to link a GL program.
GL_LIBS = -lGL -lXext -lX11

# OPTIMISATION SETTINGS:

## Very generic
OPTON = -O

## For Linux 486/Pentium, GCC 2.7.x and 2.8.x
#OPTON = -O2 -fomit-frame-pointer -fforce-addr -fforce-mem -ffast-math \
#	-m486 -malign-loops=2 -malign-jumps=2 -malign-functions=2

## For Linux, GCC previous to 2.7.0
#OPTON = -O2 -fomit-frame-pointer -fforce-addr -fforce-mem -ffast-math -m486

#OPTON = -O2 -fomit-frame-pointer -fforce-addr -fforce-mem -ffast-math \
#	-m386 -malign-loops=1 -malign-jumps=1 -malign-functions=1

## For Solaris 2, or general-purpose GCC 2.7.x
#OPTON = -O2 -fomit-frame-pointer -fforce-addr -ffast-math -Wall

## For Digital Unix v4.x, with DEC cc v5.x
#OPTON = -O4
#CFLAGS = -DTIME -std1 -verbose -w0

## generic gcc CFLAGS.  -DTIME must be included.
CFLAGS += $(CPPFLAGS) -DTIME -DMINIX=1 -D_MINIX=1 -D_POSIX_SOURCE=1


##############################################################################
# END CONFIGURATION
##############################################################################


# local directories
PROGDIR = ./pgms
SRCDIR = ./src
TESTDIR = ./testdir
RESULTDIR = ./results
TMPDIR = ./tmp
# other directories
INCLDIR = /usr/include
LIBDIR = /lib
SCRIPTS = unixbench.logo multi.sh tst.sh index.base
SOURCES = arith.c big.c context1.c \
	dummy.c execl.c \
	fstime.c hanoi.c \
	pipe.c spawn.c \
	syscall.c looper.c timeit.c time-polling.c \
	dhry_1.c dhry_2.c dhry.h whets.c ubgears.c
TESTS = sort.src cctest.c dc.dat large.txt

# ifdef GRAPHIC_TESTS
# GRAPHIC_BINS = $(PROGDIR)/ubgears
# else
GRAPHIC_BINS =
# endif

# Program binaries.
BINS = $(PROGDIR)/arithoh $(PROGDIR)/register $(PROGDIR)/short \
	$(PROGDIR)/int $(PROGDIR)/long $(PROGDIR)/float $(PROGDIR)/double \
	$(PROGDIR)/hanoi $(PROGDIR)/syscall $(PROGDIR)/context1 \
	$(PROGDIR)/pipe $(PROGDIR)/spawn $(PROGDIR)/execl \
	$(PROGDIR)/dhry2 $(PROGDIR)/dhry2reg  $(PROGDIR)/looper \
	$(PROGDIR)/fstime $(PROGDIR)/whetstone-double $(GRAPHIC_BINS)
## These compile only on some platforms...
# $(PROGDIR)/poll $(PROGDIR)/poll2 $(PROGDIR)/select

# Required non-binary files.
REQD = $(BINS) $(PROGDIR)/unixbench.logo \
	$(PROGDIR)/multi.sh $(PROGDIR)/tst.sh $(PROGDIR)/index.base \
	$(PROGDIR)/gfx-x11 \
	$(TESTDIR)/sort.src $(TESTDIR)/cctest.c $(TESTDIR)/dc.dat \
	$(TESTDIR)/large.txt

# ######################### the big ALL ############################
all: distr programs
## Ick!!!  What is this about???  How about let's not chmod everything bogusly.
#	@chmod 744 * $(SRCDIR)/* $(PROGDIR)/* $(TESTDIR)/* $(DOCDIR)/*

# ####################### a check for Run ######################
check: $(REQD)
	make all
# ##############################################################
# distribute the files out to subdirectories if they are in this one
distr:
	@echo "Checking distribution of files"
# scripts
	@if  test ! -d  $(PROGDIR) \
        ; then  \
           mkdir $(PROGDIR) \
           ; mv $(SCRIPTS) $(PROGDIR) \
        ; else \
           echo "$(PROGDIR)  exists" \
        ; fi
# C sources
	@if  test ! -d  $(SRCDIR) \
        ; then  \
           mkdir $(SRCDIR) \
           ; mv $(SOURCES) $(SRCDIR) \
        ; else \
           echo "$(SRCDIR)  exists" \
        ; fi
# test data
	@if  test ! -d  $(TESTDIR) \
        ; then  \
           mkdir $(TESTDIR) \
           ; mv $(TESTS) $(TESTDIR) \
        ; else \
           echo "$(TESTDIR)  exists" \
        ; fi
# temporary work directory
	@if  test ! -d  $(TMPDIR) \
        ; then  \
           mkdir $(TMPDIR) \
        ; else \
           echo "$(TMPDIR)  exists" \
        ; fi
# directory for results
	@if  test ! -d  $(RESULTDIR) \
        ; then  \
           mkdir $(RESULTDIR) \
        ; else \
           echo "$(RESULTDIR)  exists" \
        ; fi

programs: $(BINS)

# Individual programs
$(PROGDIR)/arithoh:	$(SRCDIR)/arith.c
	$(CC) -o $(PROGDIR)/arithoh ${CFLAGS} ${OPTON} -Darithoh $(SRCDIR)/arith.c
$(PROGDIR)/register: $(SRCDIR)/arith.c
	$(CC) -o $(PROGDIR)/register ${CFLAGS} ${OPTON} -Ddatum='register int' $(SRCDIR)/arith.c
$(PROGDIR)/short: $(SRCDIR)/arith.c
	$(CC) -o $(PROGDIR)/short ${CFLAGS} ${OPTON} -Ddatum=short $(SRCDIR)/arith.c
$(PROGDIR)/int: $(SRCDIR)/arith.c
	$(CC) -o $(PROGDIR)/int ${CFLAGS} ${OPTON} -Ddatum=int $(SRCDIR)/arith.c
$(PROGDIR)/long: $(SRCDIR)/arith.c
	$(CC) -o $(PROGDIR)/long ${CFLAGS} ${OPTON} -Ddatum=long $(SRCDIR)/arith.c
$(PROGDIR)/float: $(SRCDIR)/arith.c
	$(CC) -o $(PROGDIR)/float ${CFLAGS} ${OPTON} -Ddatum=float $(SRCDIR)/arith.c
$(PROGDIR)/double: $(SRCDIR)/arith.c
	$(CC) -o $(PROGDIR)/double ${CFLAGS} ${OPTON} -Ddatum=double $(SRCDIR)/arith.c
$(PROGDIR)/whetstone-double: $(SRCDIR)/whets.c
	$(CC) -o $(PROGDIR)/whetstone-double ${CFLAGS} ${OPTON} -DDP -DUNIX -DUNIXBENCH $(SRCDIR)/whets.c -lm
$(PROGDIR)/hanoi: $(SRCDIR)/hanoi.c
	$(CC) -o $(PROGDIR)/hanoi ${CFLAGS} ${OPTON} $(SRCDIR)/hanoi.c

$(PROGDIR)/poll: $(SRCDIR)/time-polling.c
	$(CC) -DHAS_POLL -DUNIXBENCH -o $(PROGDIR)/poll ${CFLAGS} ${OPTON} $(SRCDIR)/time-polling.c

$(PROGDIR)/poll2: $(SRCDIR)/time-polling.c
	$(CC) -DHAS_POLL2 -DUNIXBENCH -o $(PROGDIR)/poll2 ${CFLAGS} ${OPTON} $(SRCDIR)/time-polling.c

$(PROGDIR)/select: $(SRCDIR)/time-polling.c
	$(CC) -DHAS_SELECT -DUNIXBENCH -o $(PROGDIR)/select ${CFLAGS} ${OPTON} $(SRCDIR)/time-polling.c

$(PROGDIR)/fstime: $(SRCDIR)/fstime.c
	$(CC) -o $(PROGDIR)/fstime ${CFLAGS} ${OPTON} $(SRCDIR)/fstime.c

$(PROGDIR)/syscall: $(SRCDIR)/syscall.c
	$(CC) -o $(PROGDIR)/syscall ${CFLAGS} ${OPTON} $(SRCDIR)/syscall.c
$(PROGDIR)/context1: $(SRCDIR)/context1.c
	$(CC) -o $(PROGDIR)/context1 ${CFLAGS} ${OPTON} $(SRCDIR)/context1.c
$(PROGDIR)/pipe: $(SRCDIR)/pipe.c
	$(CC) -o $(PROGDIR)/pipe ${CFLAGS} ${OPTON} $(SRCDIR)/pipe.c
$(PROGDIR)/spawn: $(SRCDIR)/spawn.c
	$(CC) -o $(PROGDIR)/spawn ${CFLAGS} ${OPTON} $(SRCDIR)/spawn.c
$(PROGDIR)/execl: $(SRCDIR)/execl.c $(SRCDIR)/big.c
	$(CC) -o $(PROGDIR)/execl ${CFLAGS} ${OPTON} $(SRCDIR)/execl.c

$(PROGDIR)/dhry2: $(SRCDIR)/dhry_1.c $(SRCDIR)/dhry_2.c $(SRCDIR)/dhry.h
	cd $(SRCDIR); $(CC) -c ${CFLAGS} -DHZ=${HZ} ${OPTON} dhry_1.c
	cd $(SRCDIR); $(CC) -c ${CFLAGS} -DHZ=${HZ} ${OPTON} dhry_2.c
	$(CC) -o $(PROGDIR)/dhry2 ${CFLAGS} ${OPTON} $(SRCDIR)/dhry_1.o $(SRCDIR)/dhry_2.o
	cd $(SRCDIR); rm -f dhry_1.o dhry_2.o
$(PROGDIR)/dhry2reg: $(SRCDIR)/dhry_1.c $(SRCDIR)/dhry_2.c $(SRCDIR)/dhry.h
	cd $(SRCDIR); $(CC) -c ${CFLAGS} -DREG=register -DHZ=${HZ} ${OPTON} dhry_1.c
	cd $(SRCDIR); $(CC) -c ${CFLAGS} -DREG=register -DHZ=${HZ} ${OPTON} dhry_2.c
	$(CC) -o $(PROGDIR)/dhry2reg ${CFLAGS} ${OPTON} $(SRCDIR)/dhry_1.o $(SRCDIR)/dhry_2.o
	cd $(SRCDIR); rm -f dhry_1.o dhry_2.o

$(PROGDIR)/looper: $(SRCDIR)/looper.c
	$(CC) -o $(PROGDIR)/looper ${CFLAGS} ${OPTON} $(SRCDIR)/looper.c

$(PROGDIR)/ubgears: $(SRCDIR)/ubgears.c
	$(CC) -o $(PROGDIR)/ubgears ${CFLAGS} ${OPTON} $(SRCDIR)/ubgears.c $(GL_LIBS)

# Run the benchmarks and create the reports
run:
	sh ./Run

clean:
	rm -f $(BINS) core *~ */*~

spotless:	clean
	rm -f $(RESULTDIR)/* $(TMPDIR)/*

## END ##
