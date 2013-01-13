# rts sources
HERE=${ARCHDIR}/sys-minix
.PATH: ${HERE}

TMP=ucontextoffsets.h.tmp
CF=${HERE}/ucontextoffsets.cf

INCS+=ucontextoffsets.h

ucontext.o: ucontextoffsets.h

SRCS+=   \
	__sigreturn.S \
	_ipc.S \
	_senda.S \
	brksize.S \
	ucontext.S

ucontextoffsets.h: ${CF} 
ucontextoffsets.h: ${NETBSDSRCDIR}/sys/sys/ucontext.h
ucontextoffsets.h: ${NETBSDSRCDIR}/include/arch/${MACHINE_ARCH}/include/stackframe.h
ucontextoffsets.h: 
	${_MKTARGET_CREATE}
	cat ${CF} | \
		${TOOL_GENASSYM} -- ${CC} ${CFLAGS} ${CPPFLAGS} ${PROF} >$TMP && \
	mv -f $TMP $@

