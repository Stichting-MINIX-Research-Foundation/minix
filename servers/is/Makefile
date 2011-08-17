# Makefile for Information Server (IS)
#

.include <bsd.own.mk>

PROG=	is
SRCS=	main.c dmp.c dmp_kernel.c dmp_pm.c dmp_fs.c dmp_rs.c dmp_ds.c dmp_vm.c

DPADD+=	${LIBSYS}
LDADD+=	-lsys

MAN=

BINDIR?= /sbin

CPPFLAGS.dmp_kernel.c+=	-I${MINIXSRCDIR}
CPPFLAGS.dmp_rs.c+=	-I${MINIXSRCDIR}
CPPFLAGS.dmp_vm.c+=	-I${MINIXSRCDIR}

.if ${BUILDAVFS} == "yes"
CFLAGS+= -D_USEAVFS
.else
CFLAGS+= -D_USEVFS
.endif

.include <minix.service.mk>
