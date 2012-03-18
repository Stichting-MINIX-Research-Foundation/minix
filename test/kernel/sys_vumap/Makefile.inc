# Copied from drivers/Makefile.inc
CPPFLAGS+= -D_MINIX -D_NETBSD_SOURCE
LDADD+= -lminlib -lcompat_minix
DPADD+= ${LIBMINLIB} ${LIBCOMPAT_MINIX}
BINDIR?=/usr/sbin
