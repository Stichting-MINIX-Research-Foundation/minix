#	$NetBSD: Makefile,v 1.2 2013/04/12 18:08:10 joerg Exp $

.include <bsd.own.mk>
.include <${.CURDIR}/../../Makefile.inc>

.PATH:          ${HEIMDIST}/tools

SCRIPTS=	krb5-config
MAN=		krb5-config.1

CLEANFILES+=	krb5-config

krb5-config: krb5-config.in
	${TOOL_SED} -e "s!@PACKAGE\@!heimdal!g" \
	        -e "s!@VERSION\@!1.5pre1!g" \
	        -e "s!@prefix\@!/!g" \
	        -e "s!@exec_prefix\@!/!g" \
	        -e "s!@libdir\@!/usr/lib!g" \
	        -e "s!@includedir\@!/usr/include!g" \
	        -e "s!@LIB_crypt\@!-lcrypt!g" \
	        -e "s!@LIB_dbopen\@!!g" \
	        -e "s!@INCLUDE_hcrypto\@!!g" \
	        -e "s!@LIB_hcrypto_appl\@!-lcrypto!g" \
	        -e "s!@LIB_dlopen\@!!g" \
	        -e "s!@LIB_door_create\@!!g" \
	        -e "s!@LIB_pkinit\@!-lhx509!g" \
	        -e "s!@PTHREAD_LIBADD\@!!g" \
	        -e "s!@LIBS\@!!g" \
	        ${HEIMDIST}/tools/krb5-config.in > $@
	chmod +x $@

.include <bsd.prog.mk>
