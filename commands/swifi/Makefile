# swifi Makefile

PROG=	swifi
SRCS=	systest.c fault_model.c extra.c db_sym.c db_disasm.c \
	db_access.c read_nlist.c
CPPFLAGS+= -DCONFIG_SWIFI
DPADD+=	${LIBELF}
LDADD+=	-lelf
MAN=

.include <bsd.prog.mk>
