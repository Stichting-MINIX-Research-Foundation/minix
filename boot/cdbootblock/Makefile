# cdbootblock: secondary boot code, when booting from cd-rom

.include <bsd.own.mk>

# XXX: Can only be built with ACK currently
.include "${MINIXSRCDIR}/boot/minix.ack16.mk"
AFLAGS+=	-Was-ncc
STRIPFLAG=	-s

PROG=	cdbootblock

SRCS=		bootblock.s
.PATH:	${.CURDIR}/../bootblock
AFLAGS+=	-DCDBOOT

BINDIR=		/usr/mdec
MAN=

LIBC=	# defined, to silence bsd.*.mk

.include <bsd.prog.mk>
