# $NetBSD: Makefile,v 1.3 2013/09/11 23:04:09 joerg Exp $

USE_FORT?= yes	# network protocol library

NOLINT=		# defined

.include <bsd.own.mk>
.include <${.CURDIR}/../../Makefile.inc>

.PATH:		${HEIMDIST}/lib/kadm5 ${HEIMBASE}/include

LIB=		kadm5clnt

LIBDPLIBS+=	krb5	${.CURDIR}/../libkrb5 \
		com_err	${.CURDIR}/../libcom_err \
		hdb	${.CURDIR}/../libhdb \
		roken	${.CURDIR}/../libroken

HEIMSRCS=	kadm5_err.et

SRCS =			\
	ad.c		\
	chpass_c.c	\
	client_glue.c	\
	common_glue.c	\
	create_c.c	\
	delete_c.c	\
	destroy_c.c	\
	flush_c.c	\
	free.c		\
	get_c.c		\
	get_princs_c.c	\
	init_c.c	\
	marshall.c	\
	modify_c.c	\
	privs_c.c	\
	randkey_c.c	\
	rename_c.c	\
	send_recv.c

NO_VERSION_SCRIPT=

INCSDIR=	/usr/include/kadm5
INCS=		admin.h			\
		private.h		\
		kadm5-protos.h		\
		kadm5-private.h		\
		kadm5-pwcheck.h		\
		${COMPILE_ET_INCS}

# Prevent collision with old MIT Kerberos includes -- require manual
# intervention of the operator.
.BEGIN:
.ifmake includes
	@if [ -f ${DESTDIR}${INCSDIR}/kadm_err.h ]; then \
		echo "Error: you must first remove the MIT headers from ${DESTDIR}${INCSDIR}" >&2; \
		false; \
	fi
.endif

.include <${HEIMBASE}/Makefile.rules.inc>
.include <bsd.lib.mk>
