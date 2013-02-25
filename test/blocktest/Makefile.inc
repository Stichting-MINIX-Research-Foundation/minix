# Copied from drivers/Makefile.inc, and slightly edited.
LDADD+= -lminlib -lcompat_minix -lc
DPADD+= ${LIBMINLIB} ${LIBCOMPAT_MINIX}
BINDIR?=/usr/sbin
