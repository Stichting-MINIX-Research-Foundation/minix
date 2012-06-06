
# Makefile for arch-dependent kernel code
.include <bsd.own.mk>

HERE=${.CURDIR}/arch/${MACHINE_ARCH}
.PATH:	${HERE}
SRCS+=	mpx.S arch_clock.c arch_do_vmctl.c arch_system.c \
	do_iopenable.c do_readbios.c do_sdevio.c exception.c i8259.c io_inb.S \
	io_inl.S io_intr.S io_inw.S io_outb.S io_outl.S io_outw.S klib.S klib16.S memory.c multiboot.S \
	oxpcie.c pre_init.c protect.c

.if ${USE_ACPI} != "no"
SRCS+= acpi.c
CPPFLAGS+= -DUSE_ACPI
.endif

.if ${USE_APIC} != "no"
SRCS+= apic.c apic_asm.S
CPPFLAGS+= -DUSE_APIC
.endif

.if ${USE_DEBUGREG} != "no"
SRCS+=  breakpoints.c debugreg.S
.endif

.if ${USE_WATCHDOG} != "no"
SRCS+= arch_watchdog.c
CPPFLAGS+= -DUSE_WATCHDOG
.endif

apic_asm.d klib.d mpx.d: procoffsets.h

# It's OK to hardcode the arch as i386 here as this and procoffsets.cf
# are i386-specific.
TMP=procoffsets.h.tmp
INCLS=../include/arch/i386/include/
PROCOFFSETSCF=procoffsets.cf

procoffsets.h: $(PROCOFFSETSCF) kernel.h proc.h $(INCLS)/stackframe.h $(INCLS)/archtypes.h
	${_MKTARGET_CREATE}
	cat ${HERE}/$(PROCOFFSETSCF) | \
	  ${TOOL_GENASSYM} -- ${CC} ${CFLAGS} ${CPPFLAGS} ${PROF} >$TMP && \
	  mv -f $TMP $@ 

.ifdef CONFIG_SMP
SRCS += arch_smp.c trampoline.S
.endif


