.include <bsd.own.mk>

PROG=	zdump
SRCS=	zdump.c ialloc.c
TZDIR=	/usr/share/zoneinfo
CPPFLAGS+= -DHAVE_ADJTIME=0 -DTZDIR=\"$(TZDIR)\"
MAN=	zdump.8

.PATH: ${MINIXSRCDIR}/lib/libc/stdtime

.include <bsd.prog.mk>
