#	$NetBSD: Makefile,v 1.10 2014/01/17 16:31:45 roy Exp $

WARNS?= 4
CWARNFLAGS.clang+=	-Wno-self-assign

.include <bsd.own.mk>

BINDIR?= /usr/bin
DIST=	${.CURDIR}/../dist
.PATH:	${DIST}

PROG=	awk
SRCS=	awkgram.y b.c lex.c lib.c main.c parse.c proctab.c run.c tran.c
CPPFLAGS+=	-I${DIST} -I.
LDADD+=	-lm
.if !defined(HOSTPROG)
DPADD+=	${LIBM}
.endif
YHEADER=	yes
CWARNFLAGS+=	-Wno-pointer-sign
COPTS.run.c+=	-Wno-format-nonliteral
COPTS.tran.c+=	-Wno-format-nonliteral

# info file originally from GNU awk 3.1.3, adjusted for nawk slightly
.if !defined(__MINIX)
# Don't install on Minix, we don't have it.
.PATH:	${NETBSDSRCDIR}/external/gpl2/gawk/dist
TEXINFO=        awk.info
.endif # !defined(__MINIX)

# During the tools build (from src/tools/awk/Makefile),
# src/tools/Makefile.host changes .CURDIR back and forth between
# src/tools/awk and src/usr.bin/awk.  For some unknown reason, including
# bsd.info.mk here leads to the obj dir being created at the wrong time,
# while .CURDIR is src/usr.bin/awk.  Work around the problem by not
# including bsd.info.mk when MKINFO=no.
.if ${MKINFO} != "no"
.include <bsd.info.mk>
.endif

.include <bsd.prog.mk>
