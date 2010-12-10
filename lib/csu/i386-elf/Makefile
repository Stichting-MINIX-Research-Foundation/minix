.include <bsd.own.mk>

SRCS=		crti.S crtn.S
OBJS=		${SRCS:N*.h:R:S/$/.o/g} gcrt1.o crt1.o Scrt1.o

realall: ${OBJS}

FILES=${OBJS}
FILESDIR=${LIBDIR}
CLEANFILES=${OBJS} crt1_c.o crt1_s.o gcrt1_c.o Scrt1_c.o

gcrt1_c.o: crt1_c.c
	${CC} ${CFLAGS} -DGCRT -c -o gcrt1_c.o ${.CURDIR}/crt1_c.c

gcrt1.o: gcrt1_c.o crt1_s.o
	${LD} ${LDFLAGS} -o gcrt1.o -r crt1_s.o gcrt1_c.o

crt1.o:	crt1_c.o crt1_s.o
	${LD} ${LDFLAGS} -o crt1.o -r crt1_s.o crt1_c.o
	${OBJCOPY} --localize-symbol _start1 crt1.o

Scrt1_c.o:	crt1_c.c
	${CC} ${CFLAGS} -fPIC -DPIC -c -o Scrt1_c.o ${.CURDIR}/crt1_c.c

Scrt1.o: Scrt1_c.o crt1_s.o
	${LD} ${LDFLAGS} -o Scrt1.o -r crt1_s.o Scrt1_c.o
	${OBJCOPY} --localize-symbol _start1 Scrt1.o

.include <bsd.prog.mk>
