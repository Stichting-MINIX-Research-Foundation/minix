#	$NetBSD: Makefile,v 1.11 2006/11/20 22:06:26 dsl Exp $
#	from: @(#)Makefile	5.6 (Berkeley) 3/12/91

.ifdef SMALLPROG
CPPFLAGS+=-DSMALL
.endif

PROG=	less
CPPFLAGS+=-I${.CURDIR} -I${.CURDIR}/../lesskey -DSYSDIR=\"/etc\"
SRCS=	brac.c ch.c charset.c cmdbuf.c command.c decode.c edit.c \
	filename.c forwback.c help.c ifile.c input.c jump.c line.c \
	linenum.c lsystem.c main.c mark.c optfunc.c option.c opttbl.c \
	os.c output.c position.c prompt.c screen.c search.c signal.c \
	tags.c ttyin.c version.c
.if defined(NBSD_LIBC) && (${NBSD_LIBC} != "no")
LDADD+=	-ltermcap
DPADD+=	${LIBTERM}
.endif
MLINKS=	less.1 more.1 less.1 page.1
LINKS=	${BINDIR}/less ${BINDIR}/more ${BINDIR}/less ${BINDIR}/page

.include <bsd.prog.mk>
