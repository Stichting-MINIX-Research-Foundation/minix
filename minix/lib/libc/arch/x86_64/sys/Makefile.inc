# rts sources
HERE=${NETBSDSRCDIR}/minix/lib/libc/arch/${ARCHSUBDIR}/sys
.PATH: ${HERE}

TMP=ucontextoffsets.h.tmp
CF=${HERE}/ucontextoffsets.cf

INCS+=ucontextoffsets.h

ucontext.o: ucontextoffsets.h

SRCS+=   \
	__sigreturn.S \
	_do_kernel_call_intr.S \
	_ipc.S \
	brksize.S \
	ipc_minix_kerninfo.S \
	ucontext.S

# The INT immediates in these stubs are assembled from IPC vector macros.
# Keep the objects tied to the canonical x86_64 headers so a vector change
# cannot leave an old INT opcode in libc.a.
X86_64_IPC_VECTOR_HEADERS= \
	${NETBSDSRCDIR}/minix/include/minix/ipcconst.h \
	${NETBSDSRCDIR}/minix/include/arch/x86_64/include/ipcconst.h \
	${NETBSDSRCDIR}/minix/include/arch/x86_64/include/interrupt.h
_ipc.o _do_kernel_call_intr.o ipc_minix_kerninfo.o: ${X86_64_IPC_VECTOR_HEADERS}

ucontextoffsets.h: ${CF}
ucontextoffsets.h: ${NETBSDSRCDIR}/sys/sys/ucontext.h
ucontextoffsets.h: ${NETBSDSRCDIR}/minix/include/arch/${MACHINE_ARCH}/include/stackframe.h
ucontextoffsets.h:
	${_MKTARGET_CREATE}
	${TOOL_CAT} ${CF} | \
		${TOOL_GENASSYM} -- ${CC} ${CFLAGS:N-Wa,*} \
		${CPPFLAGS} ${PROF} ${GENASSYM_CPPFLAGS} >$TMP && \
	mv -f $TMP $@
