# boot monitor: runs in 16-bit mode
.include <bsd.own.mk>

# XXX: Can only be built with ACK currently
MINIXID=	-sep
.include "${.CURDIR}/../minix.ack16.mk"
AFLAGS+=	-Was-ncc
STRIPFLAG=	-s
LIBDIR?=	/usr/lib/i86

PROG=	boot

SRCS=		boothead.s boot.c bootimage.c rawfs.c
# Need exec_elf.c from libexec
SRCS+=		exec_elf.c
.PATH:	${.CURDIR}/.. ${MINIXSRCDIR}/lib/libexec

CPPFLAGS+=	-I${MINIXSRCDIR} -I${.CURDIR} -I${.CURDIR}/..

DPADD+=	${LIBSYS}	# for kprintf, kmalloc
LDADD+=	-lsys

BINDIR=		/usr/mdec
MAN=

.include <bsd.prog.mk>

realall: .PHONY bootsize
bootsize: boot
	${INSTALL} -S 12kb boot
