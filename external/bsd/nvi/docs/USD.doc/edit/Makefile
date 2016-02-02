#	$NetBSD: Makefile,v 1.2 2014/07/05 19:22:02 dholland Exp $
#
#	@(#)Makefile	8.1 (Berkeley) 6/8/93

.include "../../../Makefile.inc"
.PATH: ${DIST}/docs/edit

SECTION=usd
ARTICLE=edit
SRCS=	edittut.ms
MACROS=	-ms
ROFF_TBL=yes
EXTRAHTMLFILES=edit1.png edit2.png edit3.png

.include <bsd.doc.mk>

# index for versatec is different from the one in edit.tut
# because the fonts are different and entries reference page
# rather than section numbers.  if you have a typesetter
# you should just use the index in edit.tut, and ignore editvindex.

editvindex:
	${TOOL_ROFF_RAW} ${MACROS} -n22 edit.vindex
