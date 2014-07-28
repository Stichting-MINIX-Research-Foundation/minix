BINDIR?=/usr/sbin

LDADD+= -lminlib -lcompat_minix
DPADD+= ${LIBMINLIB} ${LIBCOMPAT_MINIX}

.include "../Makefile.inc"
