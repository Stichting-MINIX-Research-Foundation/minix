#	$NetBSD: Makefile,v 1.5 2010/02/20 13:43:31 joerg Exp $

HOSTPROGNAME=	${_TOOL_PREFIX}mandoc
HOST_SRCDIR=	external/bsd/mdocml/bin/mandoc

NOMAN=		# defined

.include <bsd.own.mk>
.include <bsd.sys.mk>

NETBSD_MINOR!=	${HOST_SH} ${NETBSDSRCDIR}/sys/conf/osrelease.sh -n
NETBSD_OSNAME=	"NetBSD ${NETBSD_MINOR}"

HOST_CPPFLAGS+=	-DOSNAME=${NETBSD_OSNAME:Q} -DHAVE_CONFIG_H -I.

DPSRCS+=	config.h

CLEANFILES+=	config.log config.h

config.h: config.h.pre config.h.post
	rm -f config.log
	( cat ${DISTDIR}/config.h.pre; \
	echo; \
	if $(CC) $(CFLAGS) -Werror -c ${DISTDIR}/test-strlcat.c >> config.log 2>&1; then \
		echo '#define HAVE_STRLCAT'; \
		rm test-strlcat.o; \
	fi; \
	if $(CC) $(CFLAGS) -Werror -c ${DISTDIR}/test-strlcpy.c >> config.log 2>&1; then \
		echo '#define HAVE_STRLCPY'; \
		rm test-strlcpy.o; \
	fi; \
	echo; \
	cat ${DISTDIR}/config.h.post \
	) > $@

.include "${.CURDIR}/../Makefile.host"
