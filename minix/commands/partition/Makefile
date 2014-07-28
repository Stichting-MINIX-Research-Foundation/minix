PROG=	partition
MAN=

# We need this to find our partition.h while compiling natively
# on non-Minix.
CPPFLAGS+= -I${NETBSDSRCDIR}/minix/include/arch/${MACHINE_ARCH}/include

.include <bsd.prog.mk>
