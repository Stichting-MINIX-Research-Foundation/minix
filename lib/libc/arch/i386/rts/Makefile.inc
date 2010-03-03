# rts sources
.PATH: ${.CURDIR}/arch/${ARCH}/rts

SRCS+=   \
	__sigreturn.S \
	_ipc.S \
	_senda.S \
	brksize.S
