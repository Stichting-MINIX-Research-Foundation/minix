.PATH: ${MINIXSRCDIR}/lib/nbsd_libcompat_minix/include

INCSDIR= /usr/include

INCS+= configfile.h
INCS+= 	compat/a.out.h compat/regexp.h compat/syslog.h \
	compat/pwd.h
INCS+= net/gen/netdb.h net/gen/resolv.h net/gen/nameser.h
INCS+= sys/jmp_buf.h ${MACHINE_ARCH}/compat_jmp_buf.h
INCS+= sys/sigcontext.h
