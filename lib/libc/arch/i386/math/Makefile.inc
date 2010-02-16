# math sources
.PATH: ${.CURDIR}/arch/${ARCH}/math

SRCS+=   \
	arch_compare.c \
	arch_round.c   \
	fpu_cw.s       \
	fpu_sw.s       \
	fpu_round.s    \
	fegetround.c   \
	feholdexcept.c \
	fesetround.c
