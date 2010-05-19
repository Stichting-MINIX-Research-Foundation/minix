# Makefile for arch-dependent kernel code
.include <minix.own.mk>

.PATH:	${.CURDIR}/arch/${ARCH}
SRCS+=	arch_do_vmctl.c \
	breakpoints.c \
	arch_clock.c \
	debugreg.S \
	do_int86.c \
	do_iopenable.c \
	do_readbios.c \
	do_sdevio.c \
	exception.c \
	i8259.c \
	klib.S \
	memory.c \
	oxpcie.c \
	protect.c \
	arch_system.c \
	apic.c \
	apic_asm.S \
	arch_watchdog.c
