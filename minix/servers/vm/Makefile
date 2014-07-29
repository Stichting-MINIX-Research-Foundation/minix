# Makefile for VM server
.include <bsd.own.mk>

PROG=	vm
SRCS=	main.c alloc.c utility.c exit.c fork.c break.c \
	mmap.c slaballoc.c region.c pagefaults.c pagetable.c \
	rs.c queryexit.c pb.c regionavl.c \
	mem_anon.c mem_directphys.c mem_anon_contig.c mem_shared.c	\
	mem_cache.c cache.c vfs.c mem_file.c fdref.c acl.c

.if ${MACHINE_ARCH} == "earm"
LDFLAGS+= -T ${.CURDIR}/arch/${MACHINE_ARCH}/vm.lds
.endif

DPADD+=	${LIBSYS} ${LIBEXEC}
LDADD+=	-lsys -lexec

CPPFLAGS+= -I${.CURDIR} -I${.CURDIR}/arch/${MACHINE_ARCH}
CPPFLAGS+= -I${NETBSDSRCDIR}/minix

.include "arch/${MACHINE_ARCH}/Makefile.inc"
.include <minix.service.mk>
