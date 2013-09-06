#	$NetBSD: Makefile,v 1.18 2002/08/02 15:05:57 wiz Exp $
#	@(#)Makefile	8.1 (Berkeley) 6/5/93

PROG=	reboot
DPADD=	${LIBUTIL}
LDADD=	-lutil
CPPFLAGS+=	-DSUPPORT_UTMP -DSUPPORT_UTMPX
MAN=	reboot.8
MLINKS=	reboot.8 halt.8 \
	reboot.8 poweroff.8
LINKS=	${BINDIR}/reboot ${BINDIR}/halt \
	${BINDIR}/reboot ${BINDIR}/poweroff

.include <bsd.prog.mk>
