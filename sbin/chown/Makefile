#	from: @(#)Makefile	8.1 (Berkeley) 6/6/93
#	$NetBSD: Makefile,v 1.6 2011/01/13 23:44:11 haad Exp $

PROG=	chown
CPPFLAGS+=-DSUPPORT_DOT
MAN=	chgrp.1 chown.8
SYMLINKS+=	${BINDIR}/chown /bin/chgrp

SYMLINKS+=	${BINDIR}/chown /usr/bin/chgrp
SYMLINKS+=	${BINDIR}/chown /usr/sbin/chown

.include <bsd.prog.mk>
