# rts sources
HERE=${.CURDIR}/arch/${MACHINE_ARCH}/sys-minix
.PATH: ${HERE}

TMP=ucontextoffsets.h.tmp
INCLS=../../include
ARCHINCLS=$(INCLS)/arch/i386/include/
CF=ucontextoffsets.cf

INCS+=ucontextoffsets.h

ucontext.d: ucontextoffsets.h

SRCS+=   \
	__sigreturn.S \
	_ipc.S \
	_senda.S \
	brksize.S \
	ucontext.S

ucontextoffsets.h: $(CF) $(ARCHINCLS)/stackframe.h $(INCLS)/sys/ucontext.h
	${_MKTARGET_CREATE}
	cat ${HERE}/$(CF) | \
		${TOOL_GENASSYM} -- ${CC} ${CFLAGS} ${CPPFLAGS} ${PROF} >$TMP && \
	mv -f $TMP $@

