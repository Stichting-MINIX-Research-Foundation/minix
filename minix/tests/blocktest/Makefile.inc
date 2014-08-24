# Copied from drivers/Makefile.inc, and slightly edited.
LDADD+= -lminlib 
DPADD+= ${LIBMINLIB} 
BINDIR?=/usr/sbin
