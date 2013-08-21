#	$NetBSD: Makefile,v 1.3 2011/08/11 23:15:35 joerg Exp $

MKPRIVATELIB=	yes

.include <bsd.own.mk>

LIB=		lisp
SRCS=		bytecode.c core.c debugger.c format.c hash.c helper.c \
		io.c lisp.c math.c package.c pathname.c read.c \
		regex.c require.c stream.c string.c struct.c time.c \
		write.c xedit.c

CPPFLAGS+=	-DLISP  -DLISPDIR=\"${X11LIBDIR}/xedit/lisp\" \
		-I${X11SRCDIR.xedit}/lisp/mp \
		-I${X11SRCDIR.xedit}/lisp/re \
		-I${X11SRCDIR.xedit} \
		-I${X11SRCDIR.xc}/programs

.if defined(HAVE_PCC)
# code uses gcc-specific "label as values" feature
CPPFLAGS+=	-DANSI_SOURCE
.endif

.PATH:		${X11SRCDIR.xedit}/lisp

CWARNFLAGS.clang+=	-Wno-format

.include <bsd.x11.mk>
.include <bsd.lib.mk>
