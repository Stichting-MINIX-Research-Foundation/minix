# $NetBSD: Makefile,v 1.1 2011/04/13 19:16:49 elric Exp $

.include <bsd.own.mk>
.include <${.CURDIR}/../../Makefile.inc>

.PATH:		${HEIMDIST}/lib/com_err

LIB=		com_err
SRCS=		error.c com_err.c

INCSDIR=	/usr/include/krb5
INCS=		com_err.h com_right.h

MAN=		com_err.3
MLINKS=		com_err.3 com_err_va.3 \
		com_err.3 error_message.3 \
		com_err.3 error_table_name.3 \
		com_err.3 init_error_table.3 \
		com_err.3 set_com_err_hook.3 \
		com_err.3 reset_com_err_hook.3

.if ${USETOOLS} != "yes"
.PHONY:		compile_et
includes:	compile_et
SUBDIR=		compile_et

compile_et:
	@cd ${.CURDIR}/compile_et && ${MAKE}
.endif

.include <${HEIMBASE}/Makefile.rules.inc>
.include <bsd.lib.mk>
.if ${USETOOLS} != "yes"
.include <bsd.subdir.mk>
.endif
