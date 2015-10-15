#	$NetBSD: Makefile,v 1.7 2014/07/12 20:31:04 nakayama Exp $

.include <bsd.own.mk>

PROG=	xlsclients

CPPFLAGS+=	-I${DESTDIR}${X11INCDIR} -DHAVE_STRNLEN \
		-DPACKAGE_STRING="\"xlsclients 1.1.3\""

#BUILDSYMLINKS=	xlscli.man xlsclients.man

LDADD+=	-lxcb -lxcb-atom
DPADD+=	${LIBXCB} ${LIBXCB_ATOM}

.if defined(__MINIX)
LDADD+= -lmthread
DPADD+= ${LIBMTHREAD}
.endif # defined(__MINIX)

.PATH:	${X11SRCDIR.${PROG}} ${X11SRCDIR.${PROG}}/man

.include <bsd.x11.mk>
.include <bsd.prog.mk>
