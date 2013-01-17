# Makefile for arch-dependent libsys code
.include <bsd.own.mk>

HERE=${.CURDIR}/arch/${MACHINE_ARCH}
.PATH:  ${HERE}

SRCS+=  \
        tsc_util.c

CPPFLAGS+=        -I${HERE}/../../


