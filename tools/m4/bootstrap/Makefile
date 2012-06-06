# $NetBSD: Makefile,v 1.2 2009/11/03 22:52:59 christos Exp $

FILES=parser.h parser.c tokenizer.c

all: ${FILES}

.include <bsd.own.mk>

DIST=${NETBSDSRCDIR}/usr.bin/m4

parser.c parser.h: ${DIST}/parser.y
	${YACC} -d ${DIST}/parser.y
	sed -e 's/\$$''NetBSD:\([^$$]*\)\ \$$/NetBSD:\1/' < y.tab.c > parser.c
	rm -f y.tab.c
	mv y.tab.h parser.h

tokenizer.c:	${DIST}/tokenizer.l
	${LEX} ${DIST}/tokenizer.l
	sed -e 's/\$$''NetBSD:\([^$$]*\)\ \$$/NetBSD:\1/' < lex.yy.c > ${.TARGET}
	rm -f lex.yy.c

clean::
	rm -f ${FILES}

