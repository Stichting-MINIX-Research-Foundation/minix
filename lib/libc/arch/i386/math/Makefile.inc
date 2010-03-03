# math sources
.PATH: ${.CURDIR}/arch/${ARCH}/math

SRCS+=   \
	arch_compare.c \
	arch_round.c   \
	fpu_cw.S       \
	fpu_sw.S       \
	fpu_round.S    \
	fegetround.c   \
	feholdexcept.c \
	fesetround.c
