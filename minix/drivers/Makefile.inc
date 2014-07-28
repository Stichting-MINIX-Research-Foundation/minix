BINDIR?=/usr/sbin
MAN?=

LDADD+= -lminlib -lcompat_minix
DPADD+= ${LIBMINLIB} ${LIBCOMPAT_MINIX}

.include "../Makefile.inc"
