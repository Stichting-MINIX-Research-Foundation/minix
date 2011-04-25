# jumpboot: supplementary MBR code (alternate to masterboot)

.include <bsd.own.mk>

# XXX: Can only be built with ACK currently
.include "${MINIXSRCDIR}/boot/minix.ack16.mk"
AFLAGS+=	-Was-ncc
STRIPFLAG=	-s

PROG=	jumpboot

SRCS=		jumpboot.s

BINDIR=		/usr/mdec
MAN=

LIBC=	# defined, to silence bsd.*.mk

.include <bsd.prog.mk>
