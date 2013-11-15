#	$NetBSD: Makefile.inc,v 1.3 2013/10/14 01:37:39 joerg Exp $

LIBISPRIVATE=	yes

.if !empty(LIB:Mlldb*)
CPPFLAGS+=	-I${LLDB_SRCDIR}/source
CPPFLAGS+=	-I${LLDB_SRCDIR}/source/Plugins/Process/POSIX
.endif

.include "${.PARSEDIR}/../Makefile.inc"
