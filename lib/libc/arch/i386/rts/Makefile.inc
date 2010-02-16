# rts sources
.PATH: ${.CURDIR}/arch/${ARCH}/rts

SRCS+=   \
	__sigreturn.s \
	_ipc.s \
	_senda.s \
	brksize.s
