# Makefile for Information Server (IS)
#

.include <bsd.own.mk>

PROG=	is
SRCS=	main.c dmp.c dmp_kernel.c dmp_pm.c dmp_fs.c dmp_rs.c dmp_ds.c dmp_vm.c

DPADD+=	${LIBSYS}
LDADD+=	-lsys

MAN=

BINDIR?= /sbin

CPPFLAGS.dmp_kernel.c+=	-I${NETBSDSRCDIR}
CPPFLAGS.dmp_rs.c+=	-I${NETBSDSRCDIR}
CPPFLAGS.dmp_vm.c+=	-I${NETBSDSRCDIR}

# This setting must match the kernel's, as it affects the IRQ hooks table size.
.if ${USE_APIC} != "no"
CFLAGS+= -DUSE_APIC
.endif

.include <minix.service.mk>
