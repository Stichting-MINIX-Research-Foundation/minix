#	$NetBSD: Makefile,v 1.9 2009/10/30 00:30:20 christos Exp $

HOSTPROGNAME=	${_TOOL_PREFIX}lex
HOST_SRCDIR=	external/bsd/flex/bin

.if ${HOST_OSTYPE:C/\-.*//} == "Minix"
HOST_LDFLAGS+= -static
.endif

.include "${.CURDIR}/../Makefile.host"
HOST_CPPFLAGS+=	-DM4=\"${TOOL_M4}\"

.y.c .l.c .y.h:
	@true

scan.c:
	echo '#include <initscan.c>' >$@
parse.c:
	echo '#include <initparse.c>' >$@
parse.h:
	echo '#include <initparse.h>' >$@

scan.c: parse.h

CLEANFILES+=scan.c parse.c parse.h
