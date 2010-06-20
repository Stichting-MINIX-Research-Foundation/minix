#	$OpenBSD: Makefile,v 1.5 2003/06/23 07:52:18 deraadt Exp $

PROG=	grep
SRCS=	binary.c file.c grep.c mmfile.c queue.c util.c
LINKS=	${BINDIR}/grep ${BINDIR}/egrep \
	${BINDIR}/grep ${BINDIR}/fgrep \
	${BINDIR}/grep ${BINDIR}/zgrep \
	${BINDIR}/grep ${BINDIR}/zegrep \
	${BINDIR}/grep ${BINDIR}/zfgrep \

OPSYS!= uname
MLINKS= grep.1 egrep.1 \
	grep.1 fgrep.1 \
	grep.1 zgrep.1 \
	grep.1 zegrep.1 \
	grep.1 zfgrep.1

CFLAGS+= -Wall

.if ${OPSYS} == "Minix"
CPPFLAGS+= -DNOZ -D_POSIX_SOURCE -D_MINIX
.include <minix.prog.mk>
.else
LDADD=  -lz
DPADD=  ${LIBZ}
.include <bsd.prog.mk>
.endif
