#	$NetBSD: Makefile,v 1.19 2015/01/29 19:26:20 christos Exp $
#
#	@(#)Makefile	8.1 (Berkeley) 6/6/93

# -DEXTENDED 
# 	if you want the paste & spaste macros.
.include <bsd.own.mk>

.if defined(__MINIX)
.if !empty(DBG:M-Og) || !empty(CFLAGS:M-Og) || \
	!empty(DBG:M-g) || !empty(CFLAGS:M-g)
#LSC: -Wno-maybe-uninitialized while compiling with -DNDEBUG -Og
CWARNFLAGS.gcc+=	-Wno-maybe-uninitialized
.endif
.endif # defined(__MINIX)

PROG=		m4
CPPFLAGS+=	-DEXTENDED -I${.CURDIR}/lib
SRCS=	parser.y tokenizer.l eval.c expr.c look.c main.c misc.c gnum4.c trace.c
.PATH: ${.CURDIR}/lib
SRCS+=	ohash_create_entry.c ohash_delete.c ohash_do.c ohash_entries.c \
	ohash_enum.c ohash_init.c ohash_int.h ohash_interval.c \
	ohash_lookup_interval.c ohash_lookup_memory.c ohash_qlookup.c \
	ohash_qlookupi.c
YHEADER=1
.if (${HOSTPROG:U} == "")
DPADD+=		${LIBUTIL} ${LIBL}
LDADD+=		-lutil -ll
.endif

tokenizer.o: parser.h

CLEANFILES+=parser.c parser.h tokenizer.o

.include <bsd.prog.mk>
