#	$NetBSD: Makefile,v 1.99 2015/09/07 03:44:19 uebayasi Exp $

LIB=		kern
NOPIC=		# defined
LLIBS=		# defined

.include "Makefile.libkern"
.ifndef ARCHSUBDIR
.BEGIN:
	@echo no ARCHSUBDIR for ${MACHINE_ARCH} nor ${MACHINE_CPU}
	@false
.endif

# only needed during build
libinstall::

.undef DESTDIR
.include <bsd.lib.mk>

lib${LIB}.o:: ${OBJS:O}
	@echo building standard ${LIB} library
	@rm -f lib${LIB}.o
	@${LD} -r -o lib${LIB}.o `NM=${NM} ${LORDER} ${OBJS} | ${TSORT}`
	@echo done

lib${LIB}.po:: ${POBJS:O}
	@echo building profiled ${LIB} library
	@rm -f lib${LIB}.po
	@${LD} -r -o lib${LIB}.po `NM=${NM} ${LORDER} ${POBJS} | ${TSORT}`
	@echo done

showsources: ${SRCS:O}
	@echo ${.ALLSRC}
