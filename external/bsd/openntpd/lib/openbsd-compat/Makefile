LIBISPRIVATE=yes
LIB=	openbsd-compat

.include <bsd.own.mk>
.include "${.CURDIR}/../Makefile.inc"

DIST=	${NETBSDSRCDIR}/external/bsd/openntpd/dist/openbsd-compat

.PATH.c:	${DIST}
SRCS=	asprintf.c atomicio.c bsd-arc4random.c bsd-getifaddrs.c bsd-misc.c \
	bsd-poll.c bsd-setresgid.c bsd-setresuid.c bsd-snprintf.c \
	daemon.c errx.c fake-rfc2553.c inet_pton.c port-qnx.c strlcpy.c \
	verrx.c
CPPFLAGS+=	-I${DIST} -I${DIST}/..
.include <bsd.lib.mk>
