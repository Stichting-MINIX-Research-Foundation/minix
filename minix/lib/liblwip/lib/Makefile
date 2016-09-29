
.include <bsd.own.mk>

DISTDIR=	${NETBSDSRCDIR}/minix/lib/liblwip/dist
SRCDIR=		${DISTDIR}/src

LIB=lwip

.PATH:		${SRCDIR}/core ${SRCDIR}/core/ipv4 ${SRCDIR}/core/ipv6
.include	"${.CURDIR}/core/Makefile.inc"

.PATH:		${SRCDIR}/netif
.include	"${.CURDIR}/netif/Makefile.inc"

CPPFLAGS+=	-D_MINIX_SYSTEM
CPPFLAGS+=	-I${SRCDIR}/include -I${.CURDIR}
CPPFLAGS+=	-Wno-empty-body

WARNS?= 5

.include <bsd.lib.mk>
