# Makefile for arch-dependent kernel code
.include <bsd.own.mk>

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
	io_inb.S \
	io_inl.S \
	io_intr.S \
	io_inw.S \
	io_outb.S \
	io_outl.S \
	io_outw.S \
	klib.S \
	memory.c \
	oxpcie.c \
	protect.c \
	arch_system.c \
	apic.c \
	apic_asm.S \
	arch_watchdog.c
