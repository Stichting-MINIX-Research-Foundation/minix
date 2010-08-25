.include <bsd.own.mk>

DPADD+= ${LIBAUDIODRIVER} ${LIBDRIVER} ${LIBSYS}
LDADD+= -laudiodriver -ldriver -lsys

.if exists(${.CURDIR}/../../Makefile.inc)
.include "${.CURDIR}/../../Makefile.inc"
.endif
