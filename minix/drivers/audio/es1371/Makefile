# Makefile for the ES1371 sounddriver
PROG=	es1371
SRCS=	es1371.c AC97.c pci_helper.c wait.c sample_rate_converter.c

DPADD+= ${LIBAUDIODRIVER} ${LIBCHARDRIVER} ${LIBSYS}
LDADD+= -laudiodriver -lchardriver -lsys

.include <minix.service.mk>
