.include <bsd.own.mk>

#Arch-specific sources
.PATH: ${.CURDIR}/arch/${ARCH}
SRCS+=	vm.c pagetable.c #util.S
