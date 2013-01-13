.include <bsd.own.mk>

#Arch-specific sources
.PATH: ${.CURDIR}/arch/i386
SRCS+=	pagetable.c
