BINDIR?=/usr/sbin
MAN?=

DPADD+= ${LIBAUDIODRIVER} ${LIBCHARDRIVER} ${LIBSYS} ${LIBMINLIB}
LDADD+= -laudiodriver -lchardriver -lsys -lminlib

.include "../Makefile.inc"
