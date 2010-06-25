# Makefile for Information Server (IS)
PROG=	is
SRCS=	main.c dmp.c dmp_kernel.c dmp_pm.c dmp_fs.c dmp_rs.c dmp_ds.c dmp_vm.c

DPADD+=	${LIBSYS}
LDADD+=	-lsys

MAN=

BINDIR?= /sbin

CPPFLAGS.dmp_kernel.c+=	-I${MINIXSRCDIR}
CPPFLAGS.dmp_rs.c+=	-I${MINIXSRCDIR}
CPPFLAGS.dmp_vm.c+=	-I${MINIXSRCDIR}

.include <bsd.prog.mk>
