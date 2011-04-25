# masterboot: MBR code, first to run, 16-bit mode

.include <bsd.own.mk>

# XXX: Can only be built with ACK currently
.include "${MINIXSRCDIR}/boot/minix.ack16.mk"
AFLAGS+=	-Was-ncc
STRIPFLAG=	-s


PROG=	masterboot

SRCS=		masterboot.s

BINDIR=		/usr/mdec
MAN=

LIBC=	# defined, to silence bsd.*.mk

.include <bsd.prog.mk>
