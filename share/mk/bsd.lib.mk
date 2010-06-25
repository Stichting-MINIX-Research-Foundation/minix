#	$NetBSD: bsd.lib.mk,v 1.299 2009/11/27 11:44:36 tsutsui Exp $
#	@(#)bsd.lib.mk	8.3 (Berkeley) 4/22/94

.include <bsd.init.mk>

# Pull in <bsd.sys.mk> here so we can override its .c.o rule
.include <bsd.sys.mk>

LIBISPRIVATE?=	no

##### Minix rule to make the "install" target depend on
##### "all" and "depend" targets
realinstall: realall
realall: depend

##### Basic targets
.PHONY:		libinstall
realinstall:	libinstall
clean:		cleanlib


##### LIB specific flags.
COPTS+=     ${COPTS.lib${LIB}}
CPPFLAGS+=  ${CPPFLAGS.lib${LIB}}
CXXFLAGS+=  ${CXXFLAGS.lib${LIB}}
OBJCOPTS+=  ${OBJCOPTS.lib${LIB}}
LDADD+=     ${LDADD.lib${LIB}}
LDFLAGS+=   ${LDFLAGS.lib${LIB}}
LDSTATIC+=  ${LDSTATIC.lib${LIB}}

##### Libraries that this may depend upon.
.if defined(LIBDPLIBS) && ${MKPIC} != "no"				# {
.for _lib _dir in ${LIBDPLIBS}
.if !defined(LIBDO.${_lib})
LIBDO.${_lib}!=	cd ${_dir} && ${PRINTOBJDIR}
.MAKEOVERRIDES+=LIBDO.${_lib}
.endif
LDADD+=		-L${LIBDO.${_lib}} -l${_lib}
DPADD+=		${LIBDO.${_lib}}/lib${_lib}.so
.endfor
.endif									# }

##### Build and install rules
MKDEP_SUFFIXES?=	.o .po .so .go .ln

# add additional suffixes not exported.
# .po is used for profiling object files.
# .so is used for PIC object files.
.SUFFIXES: .out .a .ln .so .po .go .o .s .S .c .cc .cpp .cxx .C .m .F .f .r .y .l .cl .p .h

CFLAGS+=	${COPTS}
OBJCFLAGS+=	${OBJCOPTS}
AFLAGS+=	${COPTS}
FFLAGS+=	${FOPTS}

.c.o:
	${_MKTARGET_COMPILE}
	${COMPILE.c} ${COPTS.${.IMPSRC:T}} ${CPUFLAGS.${.IMPSRC:T}} ${CPPFLAGS.${.IMPSRC:T}} ${.IMPSRC} -o ${.TARGET}
# .if !defined(CFLAGS) || empty(CFLAGS:M*-g*)
# 	${OBJCOPY} -x ${.TARGET}
# .endif

.cc.o .cpp.o .cxx.o .C.o:
	${_MKTARGET_COMPILE}
	${COMPILE.cc} ${COPTS.${.IMPSRC:T}} ${CPUFLAGS.${.IMPSRC:T}} ${CPPFLAGS.${.IMPSRC:T}} ${.IMPSRC} -o ${.TARGET}
# .if !defined(CFLAGS) || empty(CFLAGS:M*-g*)
# 	${OBJCOPY} -x ${.TARGET}
# .endif

.s.o:
	${_MKTARGET_COMPILE}
	${COMPILE.s} ${COPTS.${.IMPSRC:T}} ${CPUFLAGS.${.IMPSRC:T}} ${CPPFLAGS.${.IMPSRC:T}} ${.IMPSRC} -o ${.TARGET}
#	${OBJCOPY} -x ${.TARGET}

.S.o:
	${_MKTARGET_COMPILE}
	${COMPILE.S} ${COPTS.${.IMPSRC:T}} ${CPUFLAGS.${.IMPSRC:T}} ${CPPFLAGS.${.IMPSRC:T}} ${.IMPSRC} -o ${.TARGET}
#	${OBJCOPY} -x ${.TARGET}


.if defined(LIB)							# {
_LIBS=lib${LIB}.a
.endif

OBJS+=${SRCS:N*.h:N*.sh:R:S/$/.o/g}

STOBJS+=${OBJS}

LOBJS+=${LSRCS:.c=.ln} ${SRCS:M*.c:.c=.ln}

.if ${LIBISPRIVATE} != "no"
# No installation is required
libinstall::
.endif	# ${LIBISPRIVATE} == "no"					# {

ALLOBJS=

ALLOBJS+=${STOBJS}

_YLSRCS=	${SRCS:M*.[ly]:C/\..$/.c/} ${YHEADER:D${SRCS:M*.y:.y=.h}}

.NOPATH: ${ALLOBJS} ${_LIBS} ${_YLSRCS}

realall: ${SRCS} ${ALLOBJS:O} ${_LIBS}

MKARZERO?=no

#_ARFL=crs
_ARFL=cr


__archivebuild: .USE
	${_MKTARGET_BUILD}
	rm -f ${.TARGET}
#	${AR} ${_ARFL} ${.TARGET} `NM=${NM} ${LORDER} ${.ALLSRC:M*o} | ${TSORT}`
	${AR} ${_ARFL} ${.TARGET} ${.ALLSRC:M*o}

__archiveinstall: .USE
	${_MKTARGET_INSTALL}
	${INSTALL_FILE} -o ${LIBOWN} -g ${LIBGRP} -m ${LIBMODE} \
	    ${_INSTRANLIB} ${.ALLSRC} ${.TARGET}

DPSRCS+=	${_YLSRCS}
CLEANFILES+=	${_YLSRCS}

${STOBJS} ${POBJS} ${GOBJS} ${SOBJS} ${LOBJS}: ${DPSRCS}

lib${LIB}.a:: ${STOBJS} __archivebuild

cleanlib: .PHONY
	rm -f a.out [Ee]rrs mklog core *.core ${CLEANFILES}
	rm -f lib${LIB}.a ${STOBJS}

.if !target(libinstall)							# {

libinstall:: ${DESTDIR}${LIBDIR}/lib${LIB}.a
.PRECIOUS: ${DESTDIR}${LIBDIR}/lib${LIB}.a
	${_MKTARGET_INSTALL}
	 ${INSTALL_FILE} -o ${LIBOWN} -g ${LIBGRP} -m ${LIBMODE} \
	 	${.ALLSRC} ${.TARGET}

.if !defined(BUILD) && !make(all) && !make(lib${LIB}.a)
${DESTDIR}${LIBDIR}/lib${LIB}.a: .MADE
.endif
${DESTDIR}${LIBDIR}/lib${LIB}.a: lib${LIB}.a __archiveinstall

.endif	# !target(libinstall)						# }

##### Pull in related .mk logic
LINKSOWN?= ${LIBOWN}
LINKSGRP?= ${LIBGRP}
LINKSMODE?= ${LIBMODE}
.include <bsd.files.mk>
.include <bsd.inc.mk>
.include <bsd.links.mk>
.include <bsd.dep.mk>

.if ${COMPILER_TYPE} == "ack"
.include <bsd.ack.mk>
.elif ${COMPILER_TYPE} == "gnu"
.include <bsd.gcc.mk>
.endif
