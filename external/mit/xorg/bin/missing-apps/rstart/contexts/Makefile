#	$NetBSD: Makefile,v 1.1.1.1 2008/07/29 05:01:22 mrg Exp $

CFDIR=		contexts
CFFILES=	@List default x11r6
CFSED=		-e 's,_PATH,/bin:/usr/bin:/usr/pkg/bin:/usr/local/bin:${X11BINDIR},g' \
    		-e 's,_MANPATH,${X11MANDIR}:/usr/man,g'

CONFIGSYMLINKS=	x11r6 ${FILESDIR}/x \
		x11r6 ${FILESDIR}/x11 \

.include "../Makefile.rstart"

.include <bsd.x11.mk>
.include <bsd.prog.mk>
