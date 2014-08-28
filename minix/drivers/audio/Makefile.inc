BINDIR?= /service
MAN?=

DPADD+= ${LIBAUDIODRIVER} ${LIBCHARDRIVER} ${LIBSYS} 
LDADD+= -laudiodriver -lchardriver -lsys 

.include "../Makefile.inc"
