#	$NetBSD: Makefile,v 1.5 2011/08/17 15:32:20 christos Exp $
#	$FreeBSD: head/usr.bin/grep/Makefile 210389 2010-07-22 19:11:57Z gabor $
#	$OpenBSD: Makefile,v 1.6 2003/06/25 15:00:04 millert Exp $

PROG=	grep
SRCS=	fastgrep.c file.c grep.c queue.c util.c

LINKS=	${BINDIR}/grep ${BINDIR}/egrep	\
	${BINDIR}/grep ${BINDIR}/fgrep	\
	${BINDIR}/grep ${BINDIR}/zgrep	\
	${BINDIR}/grep ${BINDIR}/zegrep	\
	${BINDIR}/grep ${BINDIR}/zfgrep

MLINKS=	grep.1 egrep.1	\
	grep.1 fgrep.1	\
	grep.1 zgrep.1	\
	grep.1 zegrep.1	\
	grep.1 zfgrep.1

LDADD=	-lz -lbz2
DPADD=	${LIBZ} ${LIBBZ2}

.PATH: ${.CURDIR}/nls

NLS=	C.msg \
	es_ES.ISO8859-1.msg \
	gl_ES.ISO8859-1.msg \
	hu_HU.ISO8859-2.msg \
	ja_JP.eucJP.msg \
	ja_JP.SJIS.msg \
	ja_JP.UTF-8.msg \
	pt_BR.ISO8859-1.msg \
	ru_RU.KOI8-R.msg \
	uk_UA.UTF-8.msg \
	zh_CN.UTF-8.msg

COPTS.grep.c += -Wno-format-nonliteral
COPTS.util.c += -Wno-format-nonliteral

.include <bsd.prog.mk>
