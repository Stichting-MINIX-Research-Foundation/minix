# swifi Makefile

PROG=	swifi
SRCS=	systest.c fault_model.c extra.c db_sym.c db_disasm.c \
	db_access.c read_nlist.c
MAN=

DPADD+=	${LIBELF}
LDADD+=	-lelf

.include <bsd.prog.mk>
