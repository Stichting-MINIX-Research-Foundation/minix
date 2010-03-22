.include <minix.own.mk>

LIBCOMMON != cd ${.CURDIR}/../common && ${PRINTOBJDIR}
CPPFLAGS+=-I${.CURDIR}/../common
DPADD+= ${LIBCOMMON}/libcommon.a
LDADD+= -L${LIBCOMMON} -lcommon

DPADD+=	${LIBSYS}
LDADD+=	-lsys

.if exists(${.CURDIR}/../../Makefile.inc)
.include "${.CURDIR}/../../Makefile.inc"
.endif
