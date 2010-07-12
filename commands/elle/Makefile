# Makefile for elle

PROGS=	ellec elle
SRCS.elle= eemain.c eecmds.c eesite.c eevini.c eedisp.c eeterm.c eeerr.c  \
      eeques.c eebuff.c eefile.c eefed.c eeedit.c eebit.c eef1.c \
      eef2.c eefd.c eehelp.c eekmac.c eef3.c eesrch.c eequer.c \
      eefill.c eediag.c sbstr.c sbm.c sberr.c
SRCS.ellec= ellec.c
CPPFLAGS+= -DIGN_JOB_CONTROL -wa
MAN.elle=
MAN.ellec=

defprf.c:	deffun.e
	cat deffun.e defprf.e | ellec -Pconf  > defprf.c

eefdef.h:	deffun.e
	cat deffun.e defprf.e | ellec -Fconf  > eefdef.h

eefidx.h:	deffun.e
	cat deffun.e defprf.e | ellec -FXconf > eefidx.h

# Don't flush these files if interrupted, dammit!
.PRECIOUS: ellec deffun.e defprf.e

.include <bsd.prog.mk>
