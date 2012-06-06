# rts sources
.PATH: ${.CURDIR}/arch/${MACHINE_ARCH}/sys-minix

SRCS+=   \
	__sigreturn.S \
	_ipc.S \
	_senda.S \
	brksize.S \
	ucontext.S
