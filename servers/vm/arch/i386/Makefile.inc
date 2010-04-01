.include <minix.own.mk>

#Arch-specific sources
.PATH: ${.CURDIR}/arch/${ARCH}
SRCS+=	vm.c pagetable.c arch_pagefaults.c util.S
