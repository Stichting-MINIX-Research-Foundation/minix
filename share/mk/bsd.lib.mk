#	$NetBSD: bsd.lib.mk,v 1.324 2012/08/23 21:21:16 joerg Exp $
#	@(#)bsd.lib.mk	8.3 (Berkeley) 4/22/94

.include <bsd.init.mk>
.include <bsd.shlib.mk>
.include <bsd.gcc.mk>
# Pull in <bsd.sys.mk> here so we can override its .c.o rule
.include <bsd.sys.mk>

LIBISMODULE?=	no
LIBISPRIVATE?=	no
LIBISCXX?=	no

_LIB_PREFIX=	lib

.if ${LIBISMODULE} != "no"
_LIB_PREFIX=	# empty
MKDEBUGLIB:=	no
MKLINT:=	no
MKPICINSTALL:=	no
MKPROFILE:=	no
MKSTATICLIB:=	no
.endif

.if ${LIBISPRIVATE} != "no"
MKDEBUGLIB:=	no
MKLINT:=	no
MKPICINSTALL:=	no
. if defined(NOSTATICLIB) && ${MKPICLIB} != "no"
MKSTATICLIB:=	no
. else
MKPIC:=		no
. endif
MKPROFILE:=	no
.endif

##### Basic targets
.PHONY:		checkver libinstall
realinstall:	checkver libinstall

##### LIB specific flags.
# XXX: This is needed for programs that link with .a libraries
# Perhaps a more correct solution is to always generate _pic.a
# files or always have a shared library.
.if defined(MKPIE) && (${MKPIE} != "no")
CFLAGS+=        ${PIE_CFLAGS}
AFLAGS+=        ${PIE_AFLAGS}
.endif

##### Libraries that this may depend upon.
.if defined(LIBDPLIBS) && ${MKPIC} != "no"				# {
.for _lib _dir in ${LIBDPLIBS}
.if !defined(LIBDO.${_lib})
LIBDO.${_lib}!=	cd "${_dir}" && ${PRINTOBJDIR}
.MAKEOVERRIDES+=LIBDO.${_lib}
.endif
LDADD+=		-L${LIBDO.${_lib}} -l${_lib}
DPADD+=		${LIBDO.${_lib}}/lib${_lib}.so
.endfor
.endif									# }

##### Build and install rules
MKDEP_SUFFIXES?=	.o .po .pico .go .ln

.if !defined(SHLIB_MAJOR) && exists(${SHLIB_VERSION_FILE})		# {
SHLIB_MAJOR != . ${SHLIB_VERSION_FILE} ; echo $$major
SHLIB_MINOR != . ${SHLIB_VERSION_FILE} ; echo $$minor
SHLIB_TEENY != . ${SHLIB_VERSION_FILE} ; echo $$teeny

DPADD+=	${SHLIB_VERSION_FILE}

# Check for higher installed library versions.
.if !defined(NOCHECKVER) && !defined(NOCHECKVER_${LIB}) && \
	exists(${NETBSDSRCDIR}/lib/checkver)
checkver:
	@(cd "${.CURDIR}" && \
	    HOST_SH=${HOST_SH:Q} AWK=${TOOL_AWK:Q} \
	    ${HOST_SH} ${NETBSDSRCDIR}/lib/checkver -v ${SHLIB_VERSION_FILE} \
		    -d ${DESTDIR}${_LIBSODIR} ${LIB})
.endif
.endif									# }

.if !target(checkver)
checkver:
.endif

print-shlib-major:
.if defined(SHLIB_MAJOR) && ${MKPIC} != "no"
	@echo ${SHLIB_MAJOR}
.else
	@false
.endif

print-shlib-minor:
.if defined(SHLIB_MINOR) && ${MKPIC} != "no"
	@echo ${SHLIB_MINOR}
.else
	@false
.endif

print-shlib-teeny:
.if defined(SHLIB_TEENY) && ${MKPIC} != "no"
	@echo ${SHLIB_TEENY}
.else
	@false
.endif

.if defined(SHLIB_MAJOR) && !empty(SHLIB_MAJOR)				# {
.if defined(SHLIB_MINOR) && !empty(SHLIB_MINOR)
.if defined(SHLIB_TEENY) && !empty(SHLIB_TEENY)
SHLIB_FULLVERSION=${SHLIB_MAJOR}.${SHLIB_MINOR}.${SHLIB_TEENY}
.else
SHLIB_FULLVERSION=${SHLIB_MAJOR}.${SHLIB_MINOR}
.endif
.else
SHLIB_FULLVERSION=${SHLIB_MAJOR}
.endif
.endif									# }

# add additional suffixes not exported.
# .po is used for profiling object files.
# .pico is used for PIC object files.
.SUFFIXES: .out .a .ln .pico .po .go .o .s .S .c .cc .cpp .cxx .C .m .F .f .r .y .l .cl .p .h
.SUFFIXES: .sh .m4 .m


# Set PICFLAGS to cc flags for producing position-independent code,
# if not already set.  Includes -DPIC, if required.

# Data-driven table using make variables to control how shared libraries
# are built for different platforms and object formats.
# SHLIB_MAJOR, SHLIB_MINOR, SHLIB_TEENY: Major, minor, and teeny version
#			numbers of shared library
# SHLIB_SOVERSION:	version number to be compiled into a shared library
#			via -soname. Usualy ${SHLIB_MAJOR} on ELF.
#			NetBSD/pmax used to use ${SHLIB_MAJOR}[.${SHLIB_MINOR}
#			[.${SHLIB_TEENY}]]
# SHLIB_SHFLAGS:	Flags to tell ${LD} to emit shared library.
#			with ELF, also set shared-lib version for ld.so.
# SHLIB_LDSTARTFILE:	support .o file, call C++ file-level constructors
# SHLIB_LDENDFILE:	support .o file, call C++ file-level destructors
# FPICFLAGS:		flags for ${FC} to compile .[fF] files to .pico objects.
# CPPPICFLAGS:		flags for ${CPP} to preprocess .[sS] files for ${AS}
# CPICFLAGS:		flags for ${CC} to compile .[cC] files to pic objects.
# CSHLIBFLAGS:		flags for ${CC} to compile .[cC] files to .pico objects.
#			(usually includes ${CPICFLAGS})
# CAPICFLAGS:		flags for ${CC} to compiling .[Ss] files
#		 	(usually just ${CPPPICFLAGS} ${CPICFLAGS})
# APICFLAGS:		flags for ${AS} to assemble .[sS] to .pico objects.

.if ${MACHINE_ARCH} == "alpha"						# {

FPICFLAGS ?= -fPIC
CPICFLAGS ?= -fPIC -DPIC
CPPPICFLAGS?= -DPIC
CAPICFLAGS?= ${CPPPICFLAGS} ${CPICFLAGS}
APICFLAGS ?=

.elif (${MACHINE_ARCH} == "sparc" || ${MACHINE_ARCH} == "sparc64") 	# } {

# If you use -fPIC you need to define BIGPIC to turn on 32-bit
# relocations in asm code
FPICFLAGS ?= -fPIC
CPICFLAGS ?= -fPIC -DPIC
CPPPICFLAGS?= -DPIC -DBIGPIC
CAPICFLAGS?= ${CPPPICFLAGS} ${CPICFLAGS}
APICFLAGS ?= -KPIC

.else									# } {

# Platform-independent flags for NetBSD shared libraries
SHLIB_SOVERSION=${SHLIB_FULLVERSION}
SHLIB_SHFLAGS=
FPICFLAGS ?= -fPIC
CPICFLAGS?= -fPIC -DPIC
CPPPICFLAGS?= -DPIC
CAPICFLAGS?= ${CPPPICFLAGS} ${CPICFLAGS}
APICFLAGS?= -k

.endif									# }

.if ${MKPICLIB} != "no"
CSHLIBFLAGS+= ${CPICFLAGS}
.endif

.if defined(CSHLIBFLAGS) && !empty(CSHLIBFLAGS)
MKSHLIBOBJS= yes
.else
MKSHLIBOBJS= no
.endif

# Platform-independent linker flags for ELF shared libraries
SHLIB_SOVERSION=	${SHLIB_MAJOR}
SHLIB_SHFLAGS=		-Wl,-soname,${_LIB_PREFIX}${LIB}.so.${SHLIB_SOVERSION}
SHLIB_SHFLAGS+=		-Wl,--warn-shared-textrel
.if defined(__MINIX)
SHLIB_SHFLAGS+=		${${HAVE_GOLD:Uno} != "no":? -Wl,--script,${LDS_SHARED_LIB}:}
.endif # defined(__MINIX)
SHLIB_LDSTARTFILE?=	${_GCC_CRTI} ${_GCC_CRTBEGINS}
SHLIB_LDENDFILE?=	${_GCC_CRTENDS} ${_GCC_CRTN}

CFLAGS+=	${COPTS}
OBJCFLAGS+=	${OBJCOPTS}
AFLAGS+=	${COPTS}
FFLAGS+=	${FOPTS}

.if defined(CTFCONVERT)
.if defined(CFLAGS) && !empty(CFLAGS:M*-g*)
CTFFLAGS+=	-g
.endif
.endif

.if ${USE_BITCODE:Uno} == "yes"
.S.bc: ${.TARGET:.bc=.o}
	rm -f ${.TARGET}
	ln ${.TARGET:.bc=.o} ${.TARGET}
.c.bc:
	${_MKTARGET_COMPILE}
	${COMPILE.c} ${COPTS.${.IMPSRC:T}} ${CPUFLAGS.${.IMPSRC:T}} ${CPPFLAGS.${.IMPSRC:T}} ${.IMPSRC} -o - -Qunused-arguments -E -D"__weak_alias(X, Y)=__weak_alias2( __WEAK__ ## X, __WEAK__ ## Y)" \
	| sed 's/__WEAK__//g' \
	| ${COMPILE.c} -flto ${COPTS.${.IMPSRC:T}} ${CPUFLAGS.${.IMPSRC:T}} ${CPPFLAGS.${.IMPSRC:T}} -x c - -o ${.TARGET} -D"STRINGIFY(a)=#a" -D"__weak_alias2(X, Y)=_Pragma(STRINGIFY(weak X = Y))"
.endif # ${USE_BITCODE:Uno} == "yes"
.c.o:
	${_MKTARGET_COMPILE}
	${COMPILE.c} ${COPTS.${.IMPSRC:T}} ${CPUFLAGS.${.IMPSRC:T}} ${CPPFLAGS.${.IMPSRC:T}} ${.IMPSRC} -o ${.TARGET}
.if defined(CTFCONVERT)
	${CTFCONVERT} ${CTFFLAGS} ${.TARGET}
.endif
.if !defined(CFLAGS) || empty(CFLAGS:M*-g*)
	${OBJCOPY} ${OBJCOPYLIBFLAGS} ${.TARGET}
.endif

.c.po:
	${_MKTARGET_COMPILE}
	${COMPILE.c} ${PROFFLAGS} ${COPTS.${.IMPSRC:T}} ${CPUFLAGS.${.IMPSRC:T}} ${CPPFLAGS.${.IMPSRC:T}} -pg ${.IMPSRC} -o ${.TARGET}
.if defined(CTFCONVERT)
	${CTFCONVERT} ${CTFFLAGS} ${.TARGET}
.endif
.if !defined(CFLAGS) || empty(CFLAGS:M*-g*)
	${OBJCOPY} ${OBJCOPYLIBFLAGS} ${.TARGET}
.endif

.c.go:
	${_MKTARGET_COMPILE}
	${COMPILE.c} ${DEBUGFLAGS} ${COPTS.${.IMPSRC:T}} ${CPUFLAGS.${.IMPSRC:T}} ${CPPFLAGS.${.IMPSRC:T}} -g ${.IMPSRC} -o ${.TARGET}

.c.pico:
	${_MKTARGET_COMPILE}
	${COMPILE.c} ${COPTS.${.IMPSRC:T}} ${CPUFLAGS.${.IMPSRC:T}} ${CPPFLAGS.${.IMPSRC:T}} ${CSHLIBFLAGS} ${.IMPSRC} -o ${.TARGET}
.if !defined(CFLAGS) || empty(CFLAGS:M*-g*)
	${OBJCOPY} ${OBJCOPYLIBFLAGS} ${.TARGET}
.endif

.cc.o .cpp.o .cxx.o .C.o:
	${_MKTARGET_COMPILE}
	${COMPILE.cc} ${COPTS.${.IMPSRC:T}} ${CPUFLAGS.${.IMPSRC:T}} ${CPPFLAGS.${.IMPSRC:T}} ${.IMPSRC} -o ${.TARGET}
.if !defined(CFLAGS) || empty(CFLAGS:M*-g*)
	${OBJCOPY} ${OBJCOPYLIBFLAGS} ${.TARGET}
.endif

.cc.po .cpp.po .cxx.po .C.po:
	${_MKTARGET_COMPILE}
	${COMPILE.cc} ${PROFFLAGS} ${COPTS.${.IMPSRC:T}} ${CPUFLAGS.${.IMPSRC:T}} ${CPPFLAGS.${.IMPSRC:T}} -pg ${.IMPSRC} -o ${.TARGET}
.if !defined(CFLAGS) || empty(CFLAGS:M*-g*)
	${OBJCOPY} ${OBJCOPYLIBFLAGS} ${.TARGET}
.endif

.cc.go .cpp.go .cxx.go .C.go:
	${_MKTARGET_COMPILE}
	${COMPILE.cc} ${DEBUGFLAGS} ${COPTS.${.IMPSRC:T}} ${CPUFLAGS.${.IMPSRC:T}} ${CPPFLAGS.${.IMPSRC:T}} -g ${.IMPSRC} -o ${.TARGET}

.cc.pico .cpp.pico .cxx.pico .C.pico:
	${_MKTARGET_COMPILE}
	${COMPILE.cc} ${COPTS.${.IMPSRC:T}} ${CPUFLAGS.${.IMPSRC:T}} ${CPPFLAGS.${.IMPSRC:T}} ${CSHLIBFLAGS} ${.IMPSRC} -o ${.TARGET}
.if !defined(CFLAGS) || empty(CFLAGS:M*-g*)
	${OBJCOPY} ${OBJCOPYLIBFLAGS} ${.TARGET}
.endif

.f.o:
	${_MKTARGET_COMPILE}
	${COMPILE.f} ${.IMPSRC} -o ${.TARGET}
.if defined(CTFCONVERT)
	${CTFCONVERT} ${CTFFLAGS} ${.TARGET}
.endif
.if !defined(FOPTS) || empty(FOPTS:M*-g*)
	${OBJCOPY} ${OBJCOPYLIBFLAGS} ${.TARGET}
.endif

.f.po:
	${_MKTARGET_COMPILE}
	${COMPILE.f} ${PROFFLAGS} -pg ${.IMPSRC} -o ${.TARGET}
.if defined(CTFCONVERT)
	${CTFCONVERT} ${CTFFLAGS} ${.TARGET}
.endif
.if !defined(FOPTS) || empty(FOPTS:M*-g*)
	${OBJCOPY} ${OBJCOPYLIBFLAGS} ${.TARGET}
.endif

.f.go:
	${_MKTARGET_COMPILE}
	${COMPILE.f} ${DEBUGFLAGS} -g ${.IMPSRC} -o ${.TARGET}

.f.pico:
	${_MKTARGET_COMPILE}
	${COMPILE.f} ${FPICFLAGS} ${.IMPSRC} -o ${.TARGET}
.if !defined(FOPTS) || empty(FOPTS:M*-g*)
	${OBJCOPY} ${OBJCOPYLIBFLAGS} ${.TARGET}
.endif

.f.ln:
	${_MKTARGET_COMPILE}
	@echo Skipping lint for Fortran libraries.

.m.o:
	${_MKTARGET_COMPILE}
	${COMPILE.m} ${OBJCOPTS.${.IMPSRC:T}} ${.IMPSRC} -o ${.TARGET}
.if defined(CTFCONVERT)
	${CTFCONVERT} ${CTFFLAGS} ${.TARGET}
.endif
.if !defined(OBJCFLAGS) || empty(OBJCFLAGS:M*-g*)
	${OBJCOPY} ${OBJCOPYLIBFLAGS} ${.TARGET}
.endif

.m.po:
	${_MKTARGET_COMPILE}
	${COMPILE.m} ${PROFFLAGS} -pg ${OBJCOPTS.${.IMPSRC:T}} ${.IMPSRC} -o ${.TARGET}
.if defined(CTFCONVERT)
	${CTFCONVERT} ${CTFFLAGS} ${.TARGET}
.endif
.if !defined(OBJCFLAGS) || empty(OBJCFLAGS:M*-g*)
	${OBJCOPY} ${OBJCOPYLIBFLAGS} ${.TARGET}
.endif

.m.go:
	${_MKTARGET_COMPILE}
	${COMPILE.m} ${DEBUGFLAGS} -g ${OBJCOPTS.${.IMPSRC:T}} ${.IMPSRC} -o ${.TARGET}
.if !defined(OBJCFLAGS) || empty(OBJCFLAGS:M*-g*)
	${OBJCOPY} ${OBJCOPYLIBFLAGS} ${.TARGET}
.endif

.m.pico:
	${_MKTARGET_COMPILE}
	${COMPILE.m} ${CSHLIBFLAGS} ${OBJCOPTS.${.IMPSRC:T}} ${.IMPSRC} -o ${.TARGET}
.if !defined(OBJCFLAGS) || empty(OBJCFLAGS:M*-g*)
	${OBJCOPY} ${OBJCOPYLIBFLAGS} ${.TARGET}
.endif

.s.o:
	${_MKTARGET_COMPILE}
	${COMPILE.s} ${COPTS.${.IMPSRC:T}} ${CPUFLAGS.${.IMPSRC:T}} ${CPPFLAGS.${.IMPSRC:T}} ${.IMPSRC} -o ${.TARGET}
.if defined(CTFCONVERT)
	${CTFCONVERT} ${CTFFLAGS} ${.TARGET}
.endif
	${OBJCOPY} ${OBJCOPYLIBFLAGS} ${.TARGET}

.S.o:
	${_MKTARGET_COMPILE}
	${COMPILE.S} ${COPTS.${.IMPSRC:T}} ${CPUFLAGS.${.IMPSRC:T}} ${CPPFLAGS.${.IMPSRC:T}} ${.IMPSRC} -o ${.TARGET}
.if defined(CTFCONVERT)
	${CTFCONVERT} ${CTFFLAGS} ${.TARGET}
.endif
	${OBJCOPY} ${OBJCOPYLIBFLAGS} ${.TARGET}

.s.po:
	${_MKTARGET_COMPILE}
	${COMPILE.s} ${PROFFLAGS} ${COPTS.${.IMPSRC:T}} ${CPUFLAGS.${.IMPSRC:T}} ${CPPFLAGS.${.IMPSRC:T}} ${.IMPSRC} -o ${.TARGET}
.if defined(CTFCONVERT)
	${CTFCONVERT} ${CTFFLAGS} ${.TARGET}
.endif
	${OBJCOPY} ${OBJCOPYLIBFLAGS} ${.TARGET}

.S.po:
	${_MKTARGET_COMPILE}
	${COMPILE.S} ${PROFFLAGS} ${COPTS.${.IMPSRC:T}} ${CPUFLAGS.${.IMPSRC:T}} ${CPPFLAGS.${.IMPSRC:T}} ${.IMPSRC} -o ${.TARGET}
.if defined(CTFCONVERT)
	${CTFCONVERT} ${CTFFLAGS} ${.TARGET}
.endif
	${OBJCOPY} ${OBJCOPYLIBFLAGS} ${.TARGET}

.s.go:
	${_MKTARGET_COMPILE}
	${COMPILE.s} ${DEBUGFLAGS} ${COPTS.${.IMPSRC:T}} ${CPUFLAGS.${.IMPSRC:T}} ${CPPFLAGS.${.IMPSRC:T}} ${.IMPSRC} -o ${.TARGET}

.S.go:
	${_MKTARGET_COMPILE}
	${COMPILE.S} ${DEBUGFLAGS} ${COPTS.${.IMPSRC:T}} ${CPUFLAGS.${.IMPSRC:T}} ${CPPFLAGS.${.IMPSRC:T}} ${.IMPSRC} -o ${.TARGET}

.s.pico:
	${_MKTARGET_COMPILE}
	${COMPILE.s} ${CAPICFLAGS} ${COPTS.${.IMPSRC:T}} ${CPUFLAGS.${.IMPSRC:T}} ${CPPFLAGS.${.IMPSRC:T}} ${.IMPSRC} -o ${.TARGET}
	${OBJCOPY} ${OBJCOPYLIBFLAGS} ${.TARGET}

.S.pico:
	${_MKTARGET_COMPILE}
	${COMPILE.S} ${CAPICFLAGS} ${COPTS.${.IMPSRC:T}} ${CPUFLAGS.${.IMPSRC:T}} ${CPPFLAGS.${.IMPSRC:T}} ${.IMPSRC} -o ${.TARGET}
	${OBJCOPY} ${OBJCOPYLIBFLAGS} ${.TARGET}

.if defined(LIB)							# {
.if (${MKPIC} == "no" || (defined(LDSTATIC) && ${LDSTATIC} != "") \
	|| ${MKLINKLIB} != "no") && ${MKSTATICLIB} != "no"
_LIBS=lib${LIB}.a
.else
_LIBS=
.endif

.if ${LIBISPRIVATE} != "no" \
   && (defined(USE_COMBINE) && ${USE_COMBINE} == "yes" \
   && !defined(NOCOMBINE))						# {
.for f in ${SRCS:N*.h:N*.sh:C/\.[yl]$/.c/g}
COMBINEFLAGS.${LIB}.$f := ${CPPFLAGS.$f:D1} ${CPUFLAGS.$f:D2} ${COPTS.$f:D3} ${OBJCOPTS.$f:D4} ${CXXFLAGS.$f:D5}
.if empty(COMBINEFLAGS.${LIB}.${f}) && !defined(NOCOMBINE.$f)
COMBINESRCS+=	${f}
NODPSRCS+=	${f}
.else
OBJS+=  	${f:R:S/$/.o/}
.endif
.endfor

.if !empty(COMBINESRCS)
OBJS+=		lib${LIB}_combine.o
lib${LIB}_combine.o: ${COMBINESRCS}
	${_MKTARGET_COMPILE}
	${COMPILE.c} -MD --combine ${.ALLSRC} -o ${.TARGET}
.if !defined(CFLAGS) || empty(CFLAGS:M*-g*)
	${OBJCOPY} -x ${.TARGET}
.endif

CLEANFILES+=	lib${LIB}_combine.d

.if exists("lib${LIB}_combine.d")
.include "lib${LIB}_combine.d"
.endif
.endif   # empty(XSRCS.${LIB})
.else							# } {
OBJS+=${SRCS:N*.h:N*.sh:R:S/$/.o/g}
.endif							# }

STOBJS+=${OBJS}

LOBJS+=${LSRCS:.c=.ln} ${SRCS:M*.c:.c=.ln}

.if ${LIBISPRIVATE} != "no"
# No installation is required
libinstall::
.endif

.if ${MKDEBUGLIB} != "no"
_LIBS+=lib${LIB}_g.a
GOBJS+=${OBJS:.o=.go}
DEBUGFLAGS?=-DDEBUG
.endif

.if ${MKPROFILE} != "no"
_LIBS+=lib${LIB}_p.a
POBJS+=${OBJS:.o=.po}
PROFFLAGS?=-DGPROF -DPROF
.endif

.if ${MKPIC} != "no"							# {
.if ${MKPICLIB} == "no"
.if ${MKSHLIBOBJS} != "no"
# make _pic.a, which isn't really pic,
# since it's needed for making shared lib.
# but don't install it.
SOLIB=lib${LIB}_pic.a
SOBJS+=${OBJS:.o=.pico}
.else
SOLIB=lib${LIB}.a
.endif
.else
SOLIB=lib${LIB}_pic.a
_LIBS+=${SOLIB}
SOBJS+=${OBJS:.o=.pico}
.endif
.if defined(SHLIB_FULLVERSION)
_LIB.so:=lib${LIB}.so.${SHLIB_FULLVERSION}
.if ${MKDEBUG} != "no"
_LIB.debug:=${_LIB.so}.debug
.endif
_LIBS+=lib${LIB}.so.${SHLIB_FULLVERSION}
.endif
.endif									# }

.if ${MKLINT} != "no" && !empty(LOBJS)
_LIBS+=llib-l${LIB}.ln
.endif

ALLOBJS=
.if (${MKPIC} == "no" || (defined(LDSTATIC) && ${LDSTATIC} != "") \
	|| ${MKLINKLIB} != "no") && ${MKSTATICLIB} != "no"
ALLOBJS+=${STOBJS}
.endif
ALLOBJS+=${POBJS} ${SOBJS}
.if ${MKLINT} != "no" && !empty(LOBJS)
ALLOBJS+=${LOBJS}
.endif
.else	# !defined(LIB)							# } {
LOBJS=
SOBJS=
.endif	# !defined(LIB)							# }

_YLSRCS=	${SRCS:M*.[ly]:C/\..$/.c/} ${YHEADER:D${SRCS:M*.y:.y=.h}}

.if ${USE_BITCODE:Uno} == "yes"
_LIBS+=lib${LIB}_bc.a
.endif

.NOPATH: ${ALLOBJS} ${_LIBS} ${_YLSRCS}

realall: ${SRCS} ${ALLOBJS:O} ${_LIBS} ${_LIB.debug}

MKARZERO?=no

.if ${MKARZERO} == "yes"
_ARFL=crsD
_ARRANFL=sD
_INSTRANLIB=
.else
_ARFL=crs
_ARRANFL=s
_INSTRANLIB=${empty(PRESERVE):?-a "${RANLIB} -t":}
.endif

# If you change this, please consider reflecting the change in
# the override in sys/rump/Makefile.rump.
.if !target(__archivebuild)
__archivebuild: .USE
	${_MKTARGET_BUILD}
	rm -f ${.TARGET}
	${AR} ${_ARFL} ${.TARGET} `NM=${NM} ${LORDER} ${.ALLSRC:M*o} | ${TSORT}`
.endif

.if !target(__archiveinstall)
__archiveinstall: .USE
	${_MKTARGET_INSTALL}
	${INSTALL_FILE} -o ${LIBOWN} -g ${LIBGRP} -m ${LIBMODE} \
	    ${_INSTRANLIB} ${.ALLSRC} ${.TARGET}
.endif

__archivesymlinkpic: .USE
	${_MKTARGET_INSTALL}
	${INSTALL_SYMLINK} ${.ALLSRC} ${.TARGET}

DPSRCS+=	${_YLSRCS}
CLEANFILES+=	${_YLSRCS}

${STOBJS} ${POBJS} ${GOBJS} ${SOBJS} ${LOBJS}: ${DPSRCS}

.if ${USE_BITCODE:Uno} == "yes"
BCOBJS+=${OBJS:.o=.bc}
lib${LIB}_bc.a:: ${BCOBJS}
	${_MKTARGET_BUILD}
	rm -f ${.TARGET}
	${AR} ${_ARFL} ${.TARGET} `NM=${NM} ${LORDER} ${.ALLSRC:M*bc} | ${TSORT}`
.endif

lib${LIB}.a:: ${STOBJS} __archivebuild

lib${LIB}_p.a:: ${POBJS} __archivebuild

lib${LIB}_pic.a:: ${SOBJS} __archivebuild

lib${LIB}_g.a:: ${GOBJS} __archivebuild


_LIBLDOPTS=
.if ${SHLIBDIR} != "/usr/lib"
_LIBLDOPTS+=	-Wl,-rpath,${SHLIBDIR} \
		-L=${SHLIBDIR}
.elif ${SHLIBINSTALLDIR} != "/usr/lib"
_LIBLDOPTS+=	-Wl,-rpath-link,${DESTDIR}${SHLIBINSTALLDIR} \
		-L=${SHLIBINSTALLDIR}
.endif

# gcc -shared now adds -lc automatically. For libraries other than libc and
# libgcc* we add as a dependency the installed shared libc. For libc and
# libgcc* we avoid adding libc as a dependency by using -nostdlib. Note that
# -Xl,-nostdlib is not enough because we want to tell the compiler-driver not
# to add standard libraries, not the linker.
.if !defined(LIB)
.if !empty(LIBC_SO)
DPLIBC ?= ${DESTDIR}${LIBC_SO}
.endif
.else
.if ${LIB} != "c" && ${LIB:Mgcc*} == "" \
    && ${LIB} != "minlib" && ${LIB} != "sys" && ${LIB} != "minc" # Minix-specific libs
.if !empty(LIBC_SO)
DPLIBC ?= ${DESTDIR}${LIBC_SO}
.endif
.else
LDLIBC ?= -nodefaultlibs
.if ${LIB} == "c" || ${LIB} == "minc"
LDADD+= ${${ACTIVE_CC} == "gcc":?-lgcc:}
LDADD+= ${${ACTIVE_CC} == "clang":?-L/usr/pkg/compiler-rt/lib -lCompilerRT-Generic:}
.endif
.endif

.if defined(__MINIX)

.if ${LIB} == "m" || ${LIB} == "vtreefs"
# LSC: gcc generate calls to its runtime library for some floating point
#      operations and convertions, which are not included in libc.
LDADD+= ${${ACTIVE_CC} == "gcc":?-lgcc:}
.endif # ${LIB} == "m" || ${LIB} == "vtreefs"

.if ${LIB} == "stdc++"
# LSC we can't build this library without compiling gcc
LDADD+= -lgcc_s
DPADD+= ${DESTDIR}/usr/lib/libgcc_s.a
.endif # ${LIB} == "stdc++"
.endif #defined(__MINIX)
.endif

.if ${LIBISCXX} != "no"
LIBCC:=	${CXX}
.else
LIBCC:=	${CC}
.endif

_LDADD.lib${LIB}=	${LDADD} ${LDADD.lib${LIB}}
_LDFLAGS.lib${LIB}=	${LDFLAGS} ${LDFLAGS.lib${LIB}}

lib${LIB}.so.${SHLIB_FULLVERSION}: ${SOLIB} ${DPADD} ${DPLIBC} \
    ${SHLIB_LDSTARTFILE} ${SHLIB_LDENDFILE}
	${_MKTARGET_BUILD}
	rm -f lib${LIB}.so.${SHLIB_FULLVERSION}
	${LIBCC} ${LDLIBC} -Wl,-x -shared ${SHLIB_SHFLAGS} ${_LDFLAGS.lib${LIB}} \
	    -o ${.TARGET} ${_LIBLDOPTS} \
	    -Wl,--whole-archive ${SOLIB} -Wl,--no-whole-archive ${_LDADD.lib${LIB}} \
		${${USE_BITCODE:Uno} == "yes":? -Wl,-plugin=/usr/pkg/lib/bfd-plugins/LLVMgold.so -Wl,-plugin-opt=-disable-opt:}
#  We don't use INSTALL_SYMLINK here because this is just
#  happening inside the build directory/objdir. XXX Why does
#  this spend so much effort on libraries that aren't live??? XXX
.if defined(SHLIB_FULLVERSION) && defined(SHLIB_MAJOR) && \
    "${SHLIB_FULLVERSION}" != "${SHLIB_MAJOR}"
	${HOST_LN} -sf lib${LIB}.so.${SHLIB_FULLVERSION} lib${LIB}.so.${SHLIB_MAJOR}.tmp
	mv -f lib${LIB}.so.${SHLIB_MAJOR}.tmp lib${LIB}.so.${SHLIB_MAJOR}
.endif
	${HOST_LN} -sf lib${LIB}.so.${SHLIB_FULLVERSION} lib${LIB}.so.tmp
	mv -f lib${LIB}.so.tmp lib${LIB}.so
.if ${MKSTRIPIDENT} != "no"
	${OBJCOPY} -R .ident ${.TARGET}
.endif

.if defined(_LIB.debug)
${_LIB.debug}: ${_LIB.so}
	${_MKTARGET_CREATE}
	(  ${OBJCOPY} --only-keep-debug ${_LIB.so} ${_LIB.debug} \
	&& ${OBJCOPY} --strip-debug -p -R .gnu_debuglink \
		--add-gnu-debuglink=${_LIB.debug} ${_LIB.so} \
	) || (rm -f ${_LIB.debug}; false)
.endif

.if !empty(LOBJS)							# {
LLIBS?=		-lc
llib-l${LIB}.ln: ${LOBJS}
	${_MKTARGET_COMPILE}
	rm -f llib-l${LIB}.ln
.if defined(DESTDIR)
	${LINT} -C${LIB} ${.ALLSRC} -L${DESTDIR}/usr/libdata ${LLIBS}
.else
	${LINT} -C${LIB} ${.ALLSRC} ${LLIBS}
.endif
.endif									# }

lint: ${LOBJS}
.if defined(LOBJS) && !empty(LOBJS)
	${LINT} ${LINTFLAGS} ${LOBJS}
.endif

# If the number of entries in CLEANFILES is too large, then the
# commands in bsd.clean.mk encounter errors like "exec(/bin/sh)
# failed (Argument list too long)".  Avoid that by splitting the
# files to clean into several lists using different variable names.
# __cleanuse is an internal target in bsd.clean.mk; the way we
# use it here mimics the way it's used by the clean target in
# bsd.clean.mk.
#
clean: libclean1 libclean2 libclean3 libclean4 libclean5 libclean6
libclean1: .PHONY .MADE __cleanuse LIBCLEANFILES1
libclean2: .PHONY .MADE __cleanuse LIBCLEANFILES2
libclean3: .PHONY .MADE __cleanuse LIBCLEANFILES3
libclean4: .PHONY .MADE __cleanuse LIBCLEANFILES4
libclean5: .PHONY .MADE __cleanuse LIBCLEANFILES5
libclean6: .PHONY .MADE __cleanuse LIBCLEANFILES6
CLEANFILES+= a.out [Ee]rrs mklog *.core
# core conflicts with core/ in lib/liblwip
.if !defined(__MINIX)
CLEANFILES+=core
LIBCLEANFILES6+= lib${LIB}_bc.a ${BCOBJS} ${BCOBJS:=.tmp}
.endif
LIBCLEANFILES1+= lib${LIB}.a   ${STOBJS} ${STOBJS:=.tmp}
LIBCLEANFILES2+= lib${LIB}_p.a ${POBJS}  ${POBJS:=.tmp}
LIBCLEANFILES3+= lib${LIB}_g.a ${GOBJS}  ${GOBJS:=.tmp}
LIBCLEANFILES4+= lib${LIB}_pic.a lib${LIB}.so.* lib${LIB}.so ${_LIB.debug}
LIBCLEANFILES4+= ${SOBJS} ${SOBJS:=.tmp}
LIBCLEANFILES5+= llib-l${LIB}.ln ${LOBJS}

.if !target(libinstall)							# {
# Make sure it gets defined, in case MKPIC==no && MKLINKLIB==no
libinstall::

.if ${USE_BITCODE:Uno} == "yes"
libinstall:: ${DESTDIR}${LIBDIR}/bc/lib${LIB}.a
.PRECIOUS: ${DESTDIR}${LIBDIR}/bc/lib${LIB}.a

.if ${MKUPDATE} == "no"
.if !defined(BUILD) && !make(all) && !make(lib${LIB}_bc.a)
${DESTDIR}${LIBDIR}/bc/lib${LIB}.a! .MADE
.endif
${DESTDIR}${LIBDIR}/bc/lib${LIB}.a! lib${LIB}_bc.a __archiveinstall
.else
.if !defined(BUILD) && !make(all) && !make(lib${LIB}_bc.a)
${DESTDIR}${LIBDIR}/bc/lib${LIB}.a: .MADE
.endif
${DESTDIR}${LIBDIR}/bc/lib${LIB}.a: lib${LIB}_bc.a __archiveinstall
.endif
.endif # ${USE_BITCODE:Uno} == "yes"

.if ${MKLINKLIB} != "no" && ${MKSTATICLIB} != "no"
libinstall:: ${DESTDIR}${LIBDIR}/lib${LIB}.a
.PRECIOUS: ${DESTDIR}${LIBDIR}/lib${LIB}.a

.if ${MKUPDATE} == "no"
.if !defined(BUILD) && !make(all) && !make(lib${LIB}.a)
${DESTDIR}${LIBDIR}/lib${LIB}.a! .MADE
.endif
${DESTDIR}${LIBDIR}/lib${LIB}.a! lib${LIB}.a __archiveinstall
.else
.if !defined(BUILD) && !make(all) && !make(lib${LIB}.a)
${DESTDIR}${LIBDIR}/lib${LIB}.a: .MADE
.endif
${DESTDIR}${LIBDIR}/lib${LIB}.a: lib${LIB}.a __archiveinstall
.endif
.endif

.if ${MKPROFILE} != "no"
libinstall:: ${DESTDIR}${LIBDIR}/lib${LIB}_p.a
.PRECIOUS: ${DESTDIR}${LIBDIR}/lib${LIB}_p.a

.if ${MKUPDATE} == "no"
.if !defined(BUILD) && !make(all) && !make(lib${LIB}_p.a)
${DESTDIR}${LIBDIR}/lib${LIB}_p.a! .MADE
.endif
${DESTDIR}${LIBDIR}/lib${LIB}_p.a! lib${LIB}_p.a __archiveinstall
.else
.if !defined(BUILD) && !make(all) && !make(lib${LIB}_p.a)
${DESTDIR}${LIBDIR}/lib${LIB}_p.a: .MADE
.endif
${DESTDIR}${LIBDIR}/lib${LIB}_p.a: lib${LIB}_p.a __archiveinstall
.endif
.endif

.if ${MKDEBUGLIB} != "no"
libinstall:: ${DESTDIR}${LIBDIR}/lib${LIB}_g.a
.PRECIOUS: ${DESTDIR}${LIBDIR}/lib${LIB}_g.a

.if ${MKUPDATE} == "no"
.if !defined(BUILD) && !make(all) && !make(lib${LIB}_g.a)
${DESTDIR}${LIBDIR}/lib${LIB}_g.a! .MADE
.endif
${DESTDIR}${LIBDIR}/lib${LIB}_g.a! lib${LIB}_g.a __archiveinstall
.else
.if !defined(BUILD) && !make(all) && !make(lib${LIB}_g.a)
${DESTDIR}${LIBDIR}/lib${LIB}_g.a: .MADE
.endif
${DESTDIR}${LIBDIR}/lib${LIB}_g.a: lib${LIB}_g.a __archiveinstall
.endif
.endif

.if ${MKPIC} != "no" && ${MKPICINSTALL} != "no"
libinstall:: ${DESTDIR}${LIBDIR}/lib${LIB}_pic.a
.PRECIOUS: ${DESTDIR}${LIBDIR}/lib${LIB}_pic.a

.if ${MKUPDATE} == "no"
.if !defined(BUILD) && !make(all) && !make(lib${LIB}_pic.a)
${DESTDIR}${LIBDIR}/lib${LIB}_pic.a! .MADE
.endif
.if ${MKPICLIB} == "no"
${DESTDIR}${LIBDIR}/lib${LIB}_pic.a! lib${LIB}.a __archivesymlinkpic
.else
${DESTDIR}${LIBDIR}/lib${LIB}_pic.a! lib${LIB}_pic.a __archiveinstall
.endif
.else
.if !defined(BUILD) && !make(all) && !make(lib${LIB}_pic.a)
${DESTDIR}${LIBDIR}/lib${LIB}_pic.a: .MADE
.endif
.if ${MKPICLIB} == "no"
${DESTDIR}${LIBDIR}/lib${LIB}_pic.a: lib${LIB}.a __archivesymlinkpic
.else
${DESTDIR}${LIBDIR}/lib${LIB}_pic.a: lib${LIB}_pic.a __archiveinstall
.endif
.endif
.endif

.if ${MKPIC} != "no" && defined(SHLIB_FULLVERSION)
_LIB_SO_TGT=		${DESTDIR}${_LIBSODIR}/${_LIB_PREFIX}${LIB}.so
_LIB_SO_TGTLIBDIR=	   ${DESTDIR}${LIBDIR}/${_LIB_PREFIX}${LIB}.so

libinstall:: ${_LIB_SO_TGT}.${SHLIB_FULLVERSION}
.PRECIOUS: ${_LIB_SO_TGT}.${SHLIB_FULLVERSION}

.if ${MKUPDATE} == "no"
.if !defined(BUILD) && !make(all) && !make(lib${LIB}.so.${SHLIB_FULLVERSION})
${_LIB_SO_TGT}.${SHLIB_FULLVERSION}! .MADE
.endif
${_LIB_SO_TGT}.${SHLIB_FULLVERSION}! lib${LIB}.so.${SHLIB_FULLVERSION}
.else
.if !defined(BUILD) && !make(all) && !make(lib${LIB}.so.${SHLIB_FULLVERSION})
${_LIB_SO_TGT}.${SHLIB_FULLVERSION}: .MADE
.endif
${_LIB_SO_TGT}.${SHLIB_FULLVERSION}: lib${LIB}.so.${SHLIB_FULLVERSION}
.endif
	${_MKTARGET_INSTALL}
	${INSTALL_FILE} -o ${LIBOWN} -g ${LIBGRP} -m ${LIBMODE} \
		${.ALLSRC} ${.TARGET}
.if ${_LIBSODIR} != ${LIBDIR}
	${INSTALL_SYMLINK} -l r \
		${_LIB_SO_TGT}.${SHLIB_FULLVERSION} \
		${_LIB_SO_TGTLIBDIR}.${SHLIB_FULLVERSION}
.endif
.if defined(SHLIB_FULLVERSION) && defined(SHLIB_MAJOR) && \
    "${SHLIB_FULLVERSION}" != "${SHLIB_MAJOR}"
	${INSTALL_SYMLINK} \
		${_LIB_PREFIX}${LIB}.so.${SHLIB_FULLVERSION} \
		${_LIB_SO_TGT}.${SHLIB_MAJOR}
.if ${_LIBSODIR} != ${LIBDIR}
	${INSTALL_SYMLINK} -l r \
		${_LIB_SO_TGT}.${SHLIB_FULLVERSION} \
		${_LIB_SO_TGTLIBDIR}.${SHLIB_MAJOR}
.endif
.endif
.if ${MKLINKLIB} != "no"
	${INSTALL_SYMLINK} \
		${_LIB_PREFIX}${LIB}.so.${SHLIB_FULLVERSION} \
		${_LIB_SO_TGT}
.if ${_LIBSODIR} != ${LIBDIR}
	${INSTALL_SYMLINK} -l r \
		${_LIB_SO_TGT}.${SHLIB_FULLVERSION} \
		${_LIB_SO_TGTLIBDIR}
.endif
.endif
.endif

.if defined(_LIB.debug)
libinstall:: ${DESTDIR}${DEBUGDIR}${LIBDIR}/${_LIB.debug}
.PRECIOUS: ${DESTDIR}${DEBUGDIR}${LIBDIR}/${_LIB.debug}

${DESTDIR}${DEBUGDIR}${LIBDIR}/${_LIB.debug}: ${_LIB.debug}
	${_MKTARGET_INSTALL}
	${INSTALL_FILE} -o ${DEBUGOWN} -g ${DEBUGGRP} -m ${DEBUGMODE} \
		${.ALLSRC} ${.TARGET}
.endif

.if ${MKLINT} != "no" && !empty(LOBJS)
libinstall:: ${DESTDIR}${LINTLIBDIR}/llib-l${LIB}.ln
.PRECIOUS: ${DESTDIR}${LINTLIBDIR}/llib-l${LIB}.ln

.if ${MKUPDATE} == "no"
.if !defined(BUILD) && !make(all) && !make(llib-l${LIB}.ln)
${DESTDIR}${LINTLIBDIR}/llib-l${LIB}.ln! .MADE
.endif
${DESTDIR}${LINTLIBDIR}/llib-l${LIB}.ln! llib-l${LIB}.ln
.else
.if !defined(BUILD) && !make(all) && !make(llib-l${LIB}.ln)
${DESTDIR}${LINTLIBDIR}/llib-l${LIB}.ln: .MADE
.endif
${DESTDIR}${LINTLIBDIR}/llib-l${LIB}.ln: llib-l${LIB}.ln
.endif
	${_MKTARGET_INSTALL}
	${INSTALL_FILE} -o ${LIBOWN} -g ${LIBGRP} -m ${LIBMODE} \
		${.ALLSRC} ${DESTDIR}${LINTLIBDIR}
.endif
.endif	# !target(libinstall)						# }

##### Pull in related .mk logic
LINKSOWN?= ${LIBOWN}
LINKSGRP?= ${LIBGRP}
LINKSMODE?= ${LIBMODE}
.include <bsd.man.mk>
.include <bsd.nls.mk>
.include <bsd.files.mk>
.include <bsd.inc.mk>
.include <bsd.links.mk>
.include <bsd.dep.mk>
.include <bsd.clang-analyze.mk>
.include <bsd.clean.mk>

${TARGETS}:	# ensure existence
