# Copied from drivers/Makefile.inc, and slightly edited.
.if ${COMPILER_TYPE} == "gnu"
CPPFLAGS+= -D_MINIX -D_NETBSD_SOURCE
LDADD+= -lminlib -lcompat_minix -lc
DPADD+= ${LIBMINLIB} ${LIBCOMPAT_MINIX}
.else
CPPFLAGS+= -D_MINIX -D_POSIX_SOURCE
.endif
BINDIR?=/usr/sbin
