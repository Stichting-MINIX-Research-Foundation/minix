# Makefile for elle

CFLAGS = -O -DIGN_JOB_CONTROL -D_POSIX_SOURCE -wa
LDFLAGS= -i

all:	elle ellec

OBJ = eemain.o eecmds.o eesite.o eevini.o eedisp.o eeterm.o eeerr.o  \
      eeques.o eebuff.o eefile.o eefed.o eeedit.o eebit.o eef1.o \
      eef2.o eefd.o eehelp.o eekmac.o eef3.o eesrch.o eequer.o \
      eefill.o eediag.o sbstr.o sbm.o sberr.o sbbcpy.o

# It probably isn't necessary to make all this stuff all the time, but it
# is fairly easy and makes the whole process simpler.  If this is not done,
# the dependencies are very complicated because some of the .c and .h files
# are made dynamically.
elle:	ellec $(OBJ) $(FUN_OFILES) elle.h eesite.h
#	$(CC) $(CFLAGS) -c defprf.c	# depends on the new *.h files
	$(CC) $(LDFLAGS) -o $@ $(OBJ)
	install -S 64k $@

defprf.c:	deffun.e
	cat deffun.e defprf.e | ellec -Pconf  > defprf.c

eefdef.h:	deffun.e
	cat deffun.e defprf.e | ellec -Fconf  > eefdef.h

eefidx.h:	deffun.e
	cat deffun.e defprf.e | ellec -FXconf > eefidx.h

# Don't flush these files if interrupted, dammit!
.PRECIOUS: ellec deffun.e defprf.e

# The following files must be recompiled if eefidx.h is changed
eecmds.o eebuff.o eeerr.o eehelp.o eejust.o eemain.o eeques.o eef1.o: eefidx.h

# ELLE profile compiler.  
#	Although eefdef.h and defprf.c are included by ELLEC, they
#	are not listed as dependencies in order to avoid loops (see
#	their target entries).  That is OK because their information is not
#	used when generating the makecf files; it only furnishes default
#	values needed when an ELLE user compiles a user profile.
ellec: ellec.c
	$(CC) $(LDFLAGS) $(CFLAGS) -o $@ ellec.c
	install -S 8kw $@

install:	/usr/bin/elle /usr/bin/ellec

/usr/bin/elle:	elle
	install -cs -o bin elle $@

/usr/bin/ellec:	ellec
	install -cs -o bin ellec $@

clean:	
	rm -f *.o *.bak core elle ellec
