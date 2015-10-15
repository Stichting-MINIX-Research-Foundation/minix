#	$NetBSD: bsd.doc.mk,v 1.68 2015/08/04 08:36:14 dholland Exp $
#	@(#)bsd.doc.mk	8.1 (Berkeley) 8/14/93

.include <bsd.init.mk>

# The makefile should set these:
#   SECTION		one of usd, smm, or psd (lower-case)
#   ARTICLE		name of this document
#   SRCS		roff source files
#   DEPSRCS		additional roff source files implicitly included
#   MACROS		name(s) of roff macro packages, including the -m
#   ROFF_PIC		set to "yes" to use pic(1)
#   ROFF_EQN		set to "yes" to use eqn(1)
#   ROFF_TBL		set to "yes" to use tbl(1)
#   ROFF_REFER		set to "yes" to use refer(1)
#   EXTRAHTMLFILES	additional files emitted as part of HTML build
#
#   PAGES		unknown (XXX)
#   EXTRA		extra files to install (XXX)
#
# If there are multiple docs to be generated, set these:
#   SUBARTICLES=	name1 name2 ...
#   SRCS.name1=		roff source files
#   SRCS.name2=		more roff source files
#   SRCS.  :  =		  :
#   DEPSRCS.name1=	additional included roff source files
#   DEPSRCS.name2=	more additional included roff source files
#   DEPSRCS.  :  =	  :
#
# I'm hoping that MACROS and ROFF_* can be uniform across all
# subarticles.


# Old bsd.doc.mk files tend to invoke tbl and other preprocessors
# directly; they should be changed to set ROFF_* instead.
#
# Also they set e.g. DIR=usd/72.mydocument; this should be changed
# to SECTION=usd and ARTICLE=mydocument. The article numbers are
# no longer present in the file system and do not need to be known
# at build time.
#

# 20130908 dholland: Make sure all makefiles have been converted to the
# new scheme.
.if !defined(SECTION)
.error "bsd.doc.mk: SECTION must be defined"
.endif
.if target(paper.ps)
.error "bsd.doc.mk: target(paper.ps) is true -- this is not allowed"
.endif

# 20130908 dholland: right now we cannot generate pdf from roff sources,
# so build compressed postscript instead. XXX. (and: yech)
TOOL_ROFF_PDF?=false "No roff pdf support"
PRINTABLE=ps.gz
#PRINTABLE=ps
#PRINTABLE=pdf

# If there aren't subarticles, we generate one doc that has the same
# name as the top-level article.
SUBARTICLES?=${ARTICLE}
SRCS.${ARTICLE}?=${SRCS}
DEPSRCS.${ARTICLE}?=${DEPSRCS}

##### Build

.for SA in ${SUBARTICLES}
.if ${MKDOC} != "no"
realall: ${SA}.txt
realall: ${SA}.${PRINTABLE}
.if ${MKHTML} != "no" && ${MKGROFFHTMLDOC} != "no"
realall: ${SA}.html
.endif
.endif
.endfor # SUBARTICLES

.if defined(ROFF_PIC) && ${ROFF_PIC} != "no"
ROFFFLAGS+=-p
.endif
.if defined(ROFF_EQN) && ${ROFF_EQN} != "no"
ROFFFLAGS+=-e
.endif
.if defined(ROFF_TBL) && ${ROFF_TBL} != "no"
ROFFFLAGS+=-t
.endif
.if defined(ROFF_REFER) && ${ROFF_REFER} != "no"
ROFFFLAGS+=-R
.endif
ROFFFLAGS+=-I${.CURDIR}

.for SA in ${SUBARTICLES}

#
# Find the sources.
#
# We can't use .IMPSRC in the rules because they aren't suffix rules
# (they could be for some docs, but not others) and we can't use
# .ALLSRC because that includes DEPSRCS.
#
# As far as I know, the only ways to get the path discovered via .PATH
# are those two magic variables or the P modifier.
#
# For some reason the P modifier finds the path to a variable name,
# not the path to a word in a variable.
#

.for S in ${SRCS.${SA}}
SRCS2.${SA}+=${${S}:P}
.endfor
.for S in ${DEPSRCS.${SA}}
DEPSRCS2.${SA}+=${${S}:P}
.endfor

#
# Note: we use TOOL_ROFF_DOCASCII because TOOL_ROFF_ASCII invokes
# the nroff wrapper instead of groff directly, and that doesn't
# understand -I.
#
# We use TOOL_ROFF_DOCHTML because TOOL_ROFF_HTML uses -mdoc2html,
# which is great if it works but doesn't work with at least some of
# the non-mdoc docs. (e.g. the curses one) TOOL_ROFF_DOCHTML uses
# groff -Thtml, which produces fairly blah output but works with these
# docs. It might end up being necessary to choose one or the other on
# a per-document basis... sigh.
#

${SA}.txt: ${SRCS2.${SA}} ${DEPSRCS2.${SA}}
	${_MKTARGET_FORMAT}
	${TOOL_ROFF_DOCASCII} ${ROFFFLAGS} ${MACROS} ${PAGES} ${SRCS2.${SA}} \
		> ${.TARGET}

${SA}.ps: ${SRCS2.${SA}} ${DEPSRCS2.${SA}}
	${_MKTARGET_FORMAT}
	${TOOL_ROFF_PS} ${ROFFFLAGS} ${MACROS} ${PAGES} ${SRCS2.${SA}} \
		| ${TOOL_SED} -e '/^%%CreationDate:/d' \
		> ${.TARGET}

${SA}.pdf: ${SRCS2.${SA}} ${DEPSRCS2.${SA}}
	${_MKTARGET_FORMAT}
	${TOOL_ROFF_PDF} ${ROFFFLAGS} ${MACROS} ${PAGES} ${SRCS2.${SA}} \
		> ${.TARGET}

${SA}.html: ${SRCS2.${SA}} ${DEPSRCS2.${SA}}
	${_MKTARGET_FORMAT}
	${TOOL_ROFF_DOCHTML} ${ROFFFLAGS} ${MACROS} ${PAGES} ${SRCS2.${SA}} \
		-P -I -P ${SA} \
		> ${.TARGET}

${SA}.ps.gz: ${SA}.ps
	${TOOL_GZIP} -9 -c -n ${.ALLSRC} > ${.TARGET}

.endfor # SUBARTICLES

##### Install

DOCINST:=
.for SA in ${SUBARTICLES}
DOCINST+=${SA}.txt ${SA}.${PRINTABLE}
.if ${MKHTML} != "no" && ${MKGROFFHTMLDOC} != "no"
DOCINST+=${SA}.html
.endif
.endfor
.if ${MKHTML} != "no" && ${MKGROFFHTMLDOC} != "no"
DOCINST+=${EXTRAHTMLFILES}
.endif

.if ${MKDOC} != "no"
docinstall:
.for D in ${DOCINST}
	${_MKTARGET_INSTALL}
	${INSTALL_FILE} -o ${DOCOWN} -g ${DOCGRP} -m ${DOCMODE} ${D} \
		${DESTDIR}${DOCDIR}/${SECTION}/${ARTICLE}/${D}
.endfor
.else
docinstall: ;
.endif

.PHONY: docinstall
realinstall: docinstall

##### Clean

cleandoc:
.for SA in ${SUBARTICLES}
	rm -f ${SA}.txt ${SA}.ps ${SA}.ps.gz ${SA}.html
.endfor
	rm -f ${EXTRAHTMLFILES} [eE]rrs mklog ${CLEANFILES}

.PHONY: cleandoc
clean: cleandoc

##### Extra custom rules

.if !target(print)
print: ;
.PHONY: print
.for SA in ${SUBARTICLES}
print: print.${SA}
.PHONY: print.{SA}
print.${SA}: ${SA}.ps
	lpr -P${PRINTER} ${.ALLSRC}
.endfor
.endif

spell: ;
.PHONY: spell
.for SA in ${SUBARTICLES}
spell: spell.${SA}
.PHONY: spell.{SA}
spell.${SA}: ${SRCS2} ${DEPSRCS2}
	spell ${SRCS2} | sort | comm -23 - spell.ok > paper.spell
.endfor

##### Pull in related .mk logic

.include <bsd.obj.mk>
.include <bsd.sys.mk>

${TARGETS}:	# ensure existence
