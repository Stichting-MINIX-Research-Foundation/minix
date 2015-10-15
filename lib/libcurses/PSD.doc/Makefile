#	$NetBSD: Makefile,v 1.14 2014/07/15 13:27:58 christos Exp $
#	from: @(#)Makefile	8.2 (Berkeley) 5/23/94

SECTION=reference/ref3
ARTICLE=curses
SRCS=	Master
CEXAMPLES= ex1.gr ex2.gr life.gr twinkle1.gr twinkle2.gr win_st.gr
DEPSRCS=appen.A appen.B appen.C doc.I doc.II doc.III doc.IV fns.doc \
	intro.0 intro.1 intro.2.tbl intro.3 intro.4 intro.5 intro.6 \
	macros c_macros $(CEXAMPLES)
MACROS=	-me
CLEANFILES+= ${CEXAMPLES} intro.2.tbl

.SUFFIXES:
.SUFFIXES: .c .gr

#
# this section formats C input source into nice troffable (or nroffable)
# versions.  It uses the capabilites of "vgrind", which sets keywords in
# bold font, and comments in italics.
#

# Don't re-run vgrind unless you want to patch the output files.

.c.gr:
	${TOOL_VFONTEDPR} ${.IMPSRC} | grep -v "^'wh" > ${.TARGET}

# Unfortunately our make doesn't handle single-suffix rules.
intro.2.tbl: intro.2
	${TOOL_TBL} ${.ALLSRC} > ${.TARGET}

.include <bsd.doc.mk>
