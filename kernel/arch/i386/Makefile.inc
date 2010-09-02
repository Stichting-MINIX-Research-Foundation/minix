
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
	klib16.S \
	multiboot.S \
	memory.c \
	oxpcie.c \
	protect.c \
	arch_system.c \
	apic.c \
	apic_asm.S \
	arch_watchdog.c \
	pre_init.c \
	acpi.c

.if ${COMPILER_TYPE} == "ack"
I86CPPFLAGS =	-mi86
I86LDFLAGS =	-mi86

CPPFLAGS.klib16.S =	${I86CPPFLAGS}
LDFLAGS.klib16.S  =	${I86LDFLAGS}
.endif
