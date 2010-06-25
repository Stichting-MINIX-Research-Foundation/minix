.include <bsd.own.mk>

PROG=	zic
SRCS=	zic.c scheck.c ialloc.c
TZDIR=	/usr/share/zoneinfo
CPPFLAGS+= -DHAVE_ADJTIME=0 -DTZDIR=\"$(TZDIR)\"
MAN=	zic.8

.PATH: ${MINIXSRCDIR}/lib/libc/stdtime

.include <bsd.prog.mk>
