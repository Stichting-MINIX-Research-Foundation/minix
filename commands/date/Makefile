.include <bsd.own.mk>

PROG=	date
SRCS=	date.c logwtmp.c
TZDIR=	/usr/share/zoneinfo
CPPFLAGS+= -DHAVE_ADJTIME=0 -DTZDIR=\"$(TZDIR)\"
BINDIR= /bin

.PATH: ${MINIXSRCDIR}/lib/libc/stdtime

.include <bsd.prog.mk>
