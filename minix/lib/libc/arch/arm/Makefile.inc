C_HERE=${NETBSDSRCDIR}/minix/lib/libc/arch/${ARCHSUBDIR}
.PATH: ${C_HERE}

SRCS+= 	get_bp.S	\
	read_tsc.c
