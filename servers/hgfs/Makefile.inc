.include <minix.own.mk>

LIBHGFS != cd ${.CURDIR}/../libhgfs && ${PRINTOBJDIR}
CPPFLAGS+=-I${.CURDIR}/../libhgfs
DPADD+= ${LIBHGFS}/libhgfs.a
LDADD+= -L${LIBHGFS} -lhgfs

DPADD+=	${LIBSYS}
LDADD+=	-lsys

.if exists(${.CURDIR}/../../Makefile.inc)
.include "${.CURDIR}/../../Makefile.inc"
.endif
