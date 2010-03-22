# Makefile for Information Server (IS)
PROG=	is
SRCS=	main.c dmp.c dmp_kernel.c dmp_pm.c dmp_fs.c dmp_rs.c dmp_ds.c dmp_vm.c

DPADD+=	${LIBSYS}
LDADD+=	-lsys

MAN=

BINDIR?= /sbin

.include <minix.prog.mk>
