#	$NetBSD: Makefile,v 1.5 2014/09/10 21:44:22 christos Exp $
#
#	@(#)Makefile	8.20 (Berkeley) 8/18/96

SECTION=	reference/ref1
ARTICLE=	vi
SRCS=		vi.ref
DEPSRCS=	ex.cmd.roff set.opt.roff vi.cmd.roff ref.so index.so
# Add -U to the macros if you want to build the index since it needs
# unsafe mode for .sy
MACROS=		-me
ROFF_TBL=	yes
CLEANFILES+=	index index.so

.include <bsd.doc.mk>

# index.so is generated.
index.so: vi.ref
	${TOOL_SOELIM} -I${.CURDIR} ${.ALLSRC} | ${TOOL_TBL} | \
	    ${TOOL_ROFF_PS} ${MACROS} > /dev/null
	sed -e 's/MINUSSIGN/\\-/' \
	    -e 's/DOUBLEQUOTE/""/' \
	    -e "s/SQUOTE/'/" \
	    -e 's/ /__SPACE/g' < index | \
	sort -u '-t	' +0 -1 +1n | awk -f ${.CURDIR}/merge.awk | \
	sed -e 's/__SPACE/ /g' > $@
	rm -f index
