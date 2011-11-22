.include <bsd.own.mk>

DPADD+= ${LIBAUDIODRIVER} ${LIBCHARDRIVER} ${LIBSYS}
LDADD+= -laudiodriver -lchardriver -lsys

.if exists(${.CURDIR}/../../Makefile.inc)
.include "${.CURDIR}/../../Makefile.inc"
.endif
