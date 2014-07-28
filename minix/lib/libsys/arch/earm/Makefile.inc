# Makefile for arch-dependent libsys code
.include <bsd.own.mk>

HERE=${.CURDIR}/arch/${MACHINE_ARCH}
.PATH:  ${HERE}

SRCS+=  \
	frclock_util.c \
	spin.c \
        tsc_util.c

CPPFLAGS+=	-I${HERE}/../../
CPPFLAGS+=	-I${NETBSDSRCDIR} -I${NETBSDSRCDIR}/kernel/arch/${MACHINE_ARCH}/


