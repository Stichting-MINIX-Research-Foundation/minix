C_HERE=${NETBSDSRCDIR}/minix/lib/libc/arch/${ARCHSUBDIR}
.PATH: ${C_HERE}

SRCS+=	_cpuid.S	\
	get_bp.S 	\
	getprocessor.S	\
	read_tsc.S	\
	_cpufeature.c

