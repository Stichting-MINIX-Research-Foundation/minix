# Makefile for aal

PROG=	aal
SRCS=	archiver.c print.c rd.c rd_arhdr.c rd_unsig2.c sprint.c \
	wr_arhdr.c wr_bytes.c wr_int2.c wr_long.c wr_ranlib.c \
	format.c rd_bytes.c system.c write.c long2str.c
CPPFLAGS+= -I${.CURDIR} -DAAL -DSTB -DNDEBUG -DDISTRIBUTION

MAN=

.include <bsd.prog.mk>
