.if (${NBSD_LIBC} != "no")
CPPFLAGS+= -D_MINIX -D_NETBSD_SOURCE
LDADD+= -lminlib -lcompat_minix
DPADD+= ${LIBMINLIB} ${LIBCOMPAT_MINIX}
.else
CPPFLAGS+= -D_MINIX -D_POSIX_SOURCE
.endif
BINDIR?=/usr/sbin
