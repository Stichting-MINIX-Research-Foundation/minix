BINDIR?= /service
MAN?=

LDADD+= -lminlib 
DPADD+= ${LIBMINLIB} 

.include "../Makefile.inc"
