#	$NetBSD: Makefile,v 1.1 2008/09/02 09:25:39 christos Exp $
#
#	@(#)Makefile	8.20 (Berkeley) 8/18/96

DIR=		usd/13.viref
SRCS=		vi.ref ex.cmd.roff set.opt.roff vi.cmd.roff ref.so
MACROS=		-me
CLEANFILES+=	vi.ref.txt vi.ref.ps index index.so

all: vi.ref.txt vi.ref.ps

vi.ref.txt: vi.ref index.so
	${TOOL_SOELIM} vi.ref | ${TOOL_TBL} | ${TOOL_ROFF_ASCII} ${MACROS} > $@
	rm -f index
	chmod 444 $@

vi.ref.ps: vi.ref index.so
	${TOOL_SOELIM} vi.ref | ${TOOL_TBL} | ${TOOL_ROFF_PS} ${MACROS} > $@
	rm -f index
	chmod 444 $@

index.so: vi.ref
	# Build index.so, side-effect of building the paper.
	${TOOL_SOELIM} vi.ref | ${TOOL_TBL} | \
	    ${TOOL_ROFF_PS} ${MACROS} > /dev/null
	sed -e 's/MINUSSIGN/\\-/' \
	    -e 's/DOUBLEQUOTE/""/' \
	    -e "s/SQUOTE/'/" \
	    -e 's/ /__SPACE/g' < index | \
	sort -u '-t	' +0 -1 +1n | awk -f merge.awk | \
	sed -e 's/__SPACE/ /g' > $@
	rm -f index
	chmod 444 $@

.include <bsd.doc.mk>
