# Copied from drivers/Makefile.inc, and slightly edited.
CPPFLAGS+= -D_MINIX -D_NETBSD_SOURCE
LDADD+= -lminlib -lcompat_minix -lc
DPADD+= ${LIBMINLIB} ${LIBCOMPAT_MINIX}
BINDIR?=/usr/sbin
