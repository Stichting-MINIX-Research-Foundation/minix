# misc sources
.PATH: ${.CURDIR}/arch/${ARCH}/misc

SRCS+=  \
	_cpuid.S \
	_cpufeature.c \
	alloca.S \
	get_bp.S \
	getprocessor.S \
	oneC_sum.S \
	ucontext.S

