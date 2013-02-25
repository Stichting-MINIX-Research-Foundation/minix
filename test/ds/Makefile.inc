# Copied from drivers/Makefile.inc
LDADD+= -lminlib -lcompat_minix
DPADD+= ${LIBMINLIB} ${LIBCOMPAT_MINIX}
BINDIR?=/usr/sbin
