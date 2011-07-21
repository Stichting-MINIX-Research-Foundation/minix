#	$NetBSD: bsd.doc.mk,v 1.64 2006/03/16 18:43:34 jwise Exp $
#	@(#)bsd.doc.mk	8.1 (Berkeley) 8/14/93

.include <bsd.init.mk>

##### Basic targets
clean:		cleandoc
realinstall:	docinstall

##### Build rules
.if !target(paper.ps)
paper.ps: ${SRCS}
	${_MKTARGET_FORMAT}
	${TOOL_ROFF_PS} ${MACROS} ${PAGES} ${.ALLSRC} > ${.TARGET}
.endif

.if ${MKSHARE} != "no"
realall:	paper.ps
.endif

##### Install rules
docinstall::	# ensure existence
.PHONY:		docinstall

.if ${MKDOC} != "no"

__docinstall: .USE
	${_MKTARGET_INSTALL}
	${INSTALL_FILE} -o ${DOCOWN} -g ${DOCGRP} -m ${DOCMODE} \
		${.ALLSRC} ${.TARGET}

FILES?=		${SRCS}

.for F in Makefile ${FILES:O:u} ${EXTRA}
_F:=		${DESTDIR}${DOCDIR}/${DIR}/${F}		# installed path

.if ${MKUPDATE} == "no"
${_F}!		${F} __docinstall			# install rule
.if !defined(BUILD) && !make(all) && !make(${F})
${_F}!		.MADE					# no build at install
.endif
.else
${_F}:		${F} __docinstall			# install rule
.if !defined(BUILD) && !make(all) && !make(${F})
${_F}:		.MADE					# no build at install
.endif
.endif

docinstall::	${_F}
.PRECIOUS:	${_F}					# keep if install fails
.endfor

.undef _F
.endif # ${MKDOC} != "no"

##### Clean rules
cleandoc: .PHONY
	rm -f paper.* [eE]rrs mklog ${CLEANFILES}

##### Custom rules
.if !target(print)
print: .PHONY paper.ps
	lpr -P${PRINTER} ${.ALLSRC}
.endif

spell: .PHONY ${SRCS}
	spell ${.ALLSRC} | sort | comm -23 - spell.ok > paper.spell

##### Pull in related .mk logic
.include <bsd.obj.mk>
.include <bsd.sys.mk>

${TARGETS}:	# ensure existence
