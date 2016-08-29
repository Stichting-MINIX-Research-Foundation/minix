#	$NetBSD: Makefile,v 1.4 2008/09/09 06:31:33 lukem Exp $

NOMAN=
MAKEDOC=

.include "../Makefile.inc"

.if defined(__MINIX)
#LSC: First program within texinfo: trigger fetch.sh script
MODULE=		texinfo
GNUHOSTDIST=	${DIST}/${MODULE}
.include "${.CURDIR}/../../../../minix/Makefile.fetchgnu"
.endif # defined(__MINIX)

HOSTPROG=	makedoc
SRCS=		makedoc.c xexit.c xmalloc.c xstrdup.c

.PATH: ${IDIST}/info ${IDIST}/lib
HOST_CPPFLAGS+= -I${IDIST}/info -I${IDIST}/lib -I${.CURDIR}/../common
HOST_CPPFLAGS+= -DHOSTTOOL

.include <bsd.hostprog.mk>

.if defined(__MINIX) && ${USETOOLS} != "yes"
# MINIX: make sure sources are fetched, even when tools not built earlier
${SRCS} realdepend realall realinstall: ${fetch_done}
.endif # defined(__MINIX)
