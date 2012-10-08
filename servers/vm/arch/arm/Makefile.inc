.include <bsd.own.mk>

#Arch-specific sources
.PATH: ${.CURDIR}/arch/${MACHINE_ARCH}
SRCS+=	pagetable.c #util.S
