# Makefile for the tps65950 PMIC found on the BeagleBoard-xM.
PROG=   tps65950
SRCS=   tps65950.c tps65950.h rtc.c rtc.h

DPADD+= ${LIBI2CDRIVER} ${LIBSYS} ${LIBTIMERS}
LDADD+= -li2cdriver -lsys -ltimers

CPPFLAGS+=      -I${NETBSDSRCDIR}

.include <minix.service.mk>
