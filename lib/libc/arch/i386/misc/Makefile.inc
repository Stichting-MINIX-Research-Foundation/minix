# misc sources
.PATH: ${.CURDIR}/arch/${ARCH}/misc

SRCS+=  \
	_cpuid.S \
	_cpufeature.c \
	alloca.S \
	get_bp.S \
	getprocessor.S \
	io_inb.S \
	io_inl.S \
	io_insb.S \
	io_insl.S \
	io_insw.S \
	io_intr.S \
	io_inw.S \
	io_outb.S \
	io_outl.S \
	io_outsb.S \
	io_outsl.S \
	io_outsw.S \
	io_outw.S \
	oneC_sum.S \
	ucontext.S

