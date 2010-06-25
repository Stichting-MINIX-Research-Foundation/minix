PROG=	top
CPPFLAGS+= -I${MINIXSRCDIR} -I${MINIXSRCDIR}/servers
BINMODE= 4755
MAN=

DPADD+= ${LIBCURSES}
LDADD+= -lcurses

.include <bsd.prog.mk>
