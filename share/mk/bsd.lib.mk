#	$NetBSD: bsd.lib.mk,v 1.299 2009/11/27 11:44:36 tsutsui Exp $
#	@(#)bsd.lib.mk	8.3 (Berkeley) 4/22/94

.include <bsd.init.mk>
.include <bsd.shlib.mk>
#.include <bsd.gcc.mk>
# Pull in <bsd.sys.mk> here so we can override its .c.o rule
.include <bsd.sys.mk>

LIBISMODULE?=	no
LIBISPRIVATE?=	no
LIBISCXX?=	no

# Build shared libraries if not set to no by now
MKPIC?=		yes

# If we're making a library but aren't making shared
# libraries and it's because there's a non-shared-aware clang
# installed, warn the user that that's the reason.
.if $(MKPIC) == "no" && defined(NONPICCLANG)
.warning Old clang, not building shared.
.endif

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
clean:		cleanlib

##### LIB specific flags.
# XXX: This is needed for programs that link with .a libraries
# Perhaps a more correct solution is to always generate _pic.a
# files or always have a shared library.
.if defined(MKPIE) && (${MKPIE} != "no")
CFLAGS+=        ${PIE_CFLAGS}
AFLAGS+=        ${PIE_AFLAGS}
.endif
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

# Use purely kernel private headers in rump builds
.if !defined(RUMPKERNEL)
.if empty(CPPFLAGS:M-nostdinc)
CPPFLAGS+=	${DESTDIR:D-nostdinc ${CPPFLAG_ISYSTEM} ${DESTDIR}/usr/include}
.endif
.if empty(CXXFLAGS:M-nostdinc++)
CXXFLAGS+=	${DESTDIR:D-nostdinc++ ${CPPFLAG_ISYSTEMXX} ${DESTDIR}/usr/include/g++}
.endif
.endif

.if !defined(SHLIB_MAJOR) && exists(${SHLIB_VERSION_FILE})		# {
SHLIB_MAJOR != . ${SHLIB_VERSION_FILE} ; echo $$major
SHLIB_MINOR != . ${SHLIB_VERSION_FILE} ; echo $$minor
SHLIB_TEENY != . ${SHLIB_VERSION_FILE} ; echo $$teeny

DPADD+=	${SHLIB_VERSION_FILE}

# Check for higher installed library versions.
.if !defined(NOCHECKVER) && !defined(NOCHECKVER_${LIB}) && \
	exists(${NETBSDSRCDIR}/lib/checkver)
checkver:
	@(cd ${.CURDIR} && \
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
# .so is used for PIC object files.
.SUFFIXES: .out .a .ln .so .po .go .o .s .S .c .cc .cpp .cxx .C .m .F .f .r .y .l .cl .p .h
.SUFFIXES: .sh .m4 .m


# Set PICFLAGS to cc flags for producing position-independent code,
# if not already set.  Includes -DPIC, if required.

# Data-driven table using make variables to control how shared libraries
# are built for different platforms and object formats.
# OBJECT_FMT:		currently either "ELF" or "a.out", from <bsd.own.mk>
# SHLIB_SOVERSION:	version number to be compiled into a shared library
#			via -soname. Usualy ${SHLIB_MAJOR} on ELF.
#			NetBSD/pmax used to use ${SHLIB_MAJOR}[.${SHLIB_MINOR}
#			[.${SHLIB_TEENY}]]
# SHLIB_SHFLAGS:	Flags to tell ${LD} to emit shared library.
#			with ELF, also set shared-lib version for ld.so.
# SHLIB_LDSTARTFILE:	support .o file, call C++ file-level constructors
# SHLIB_LDENDFILE:	support .o file, call C++ file-level destructors
# FPICFLAGS:		flags for ${FC} to compile .[fF] files to .so objects.
# CPPPICFLAGS:		flags for ${CPP} to preprocess .[sS] files for ${AS}
# CPICFLAGS:		flags for ${CC} to compile .[cC] files to pic objects.
# CSHLIBFLAGS:		flags for ${CC} to compile .[cC] files to .so objects.
#			(usually includes ${CPICFLAGS})
# CAPICFLAGS:		flags for ${CC} to compiling .[Ss] files
#		 	(usually just ${CPPPICFLAGS} ${CPICFLAGS})
# APICFLAGS:		flags for ${AS} to assemble .[sS] to .so objects.

.if ${MACHINE_ARCH} == "alpha"						# {

FPICFLAGS ?= -fPIC
CPICFLAGS ?= -fPIC -DPIC
CPPPICFLAGS?= -DPIC
CAPICFLAGS?= ${CPPPICFLAGS} ${CPICFLAGS}
APICFLAGS ?=

.elif (${MACHINE_ARCH} == "sparc" || ${MACHINE_ARCH} == "sparc64") && \
       ${OBJECT_FMT} == "ELF"						# } {

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
.if ${OBJECT_FMT} == "ELF"
SHLIB_SOVERSION=	${SHLIB_MAJOR}
SHLIB_SHFLAGS=		-Wl,-soname,${_LIB_PREFIX}${LIB}.so.${SHLIB_SOVERSION}
SHLIB_SHFLAGS+=		-Wl,--warn-shared-textrel
SHLIB_LDSTARTFILE?=	${DESTDIR}/usr/lib/crti.o ${_GCC_CRTBEGINS}
SHLIB_LDENDFILE?=	${_GCC_CRTENDS} ${DESTDIR}/usr/lib/crtn.o
.endif

CFLAGS+=	${COPTS}
OBJCFLAGS+=	${OBJCOPTS}
AFLAGS+=	${COPTS}
FFLAGS+=	${FOPTS}

.c.o:
	${_MKTARGET_COMPILE}
	${COMPILE.c} ${COPTS.${.IMPSRC:T}} ${CPUFLAGS.${.IMPSRC:T}} ${CPPFLAGS.${.IMPSRC:T}} ${.IMPSRC} -o ${.TARGET}
.if !defined(CFLAGS) || empty(CFLAGS:M*-g*)
	${OBJCOPY} -x ${.TARGET}
.endif

.c.po:
	${_MKTARGET_COMPILE}
	${COMPILE.c} ${PROFFLAGS} ${COPTS.${.IMPSRC:T}} ${CPUFLAGS.${.IMPSRC:T}} ${CPPFLAGS.${.IMPSRC:T}} -pg ${.IMPSRC} -o ${.TARGET}
.if !defined(CFLAGS) || empty(CFLAGS:M*-g*)
	${OBJCOPY} -X ${.TARGET}
.endif

.c.go:
	${_MKTARGET_COMPILE}
	${COMPILE.c} ${DEBUGFLAGS} ${COPTS.${.IMPSRC:T}} ${CPUFLAGS.${.IMPSRC:T}} ${CPPFLAGS.${.IMPSRC:T}} -g ${.IMPSRC} -o ${.TARGET}

.c.so:
	${_MKTARGET_COMPILE}
	${COMPILE.c} ${COPTS.${.IMPSRC:T}} ${CPUFLAGS.${.IMPSRC:T}} ${CPPFLAGS.${.IMPSRC:T}} ${CSHLIBFLAGS} ${.IMPSRC} -o ${.TARGET}
.if !defined(CFLAGS) || empty(CFLAGS:M*-g*)
	${OBJCOPY} -x ${.TARGET}
.endif

.cc.o .cpp.o .cxx.o .C.o:
	${_MKTARGET_COMPILE}
	${COMPILE.cc} ${COPTS.${.IMPSRC:T}} ${CPUFLAGS.${.IMPSRC:T}} ${CPPFLAGS.${.IMPSRC:T}} ${.IMPSRC} -o ${.TARGET}
.if !defined(CFLAGS) || empty(CFLAGS:M*-g*)
	${OBJCOPY} -x ${.TARGET}
.endif

.cc.po .cpp.po .cxx.po .C.po:
	${_MKTARGET_COMPILE}
	${COMPILE.cc} ${PROFFLAGS} ${COPTS.${.IMPSRC:T}} ${CPUFLAGS.${.IMPSRC:T}} ${CPPFLAGS.${.IMPSRC:T}} -pg ${.IMPSRC} -o ${.TARGET}
.if !defined(CFLAGS) || empty(CFLAGS:M*-g*)
	${OBJCOPY} -X ${.TARGET}
.endif

.cc.go .cpp.go .cxx.go .C.go:
	${_MKTARGET_COMPILE}
	${COMPILE.cc} ${DEBUGFLAGS} ${COPTS.${.IMPSRC:T}} ${CPUFLAGS.${.IMPSRC:T}} ${CPPFLAGS.${.IMPSRC:T}} -g ${.IMPSRC} -o ${.TARGET}

.cc.so .cpp.so .cxx.so .C.so:
	${_MKTARGET_COMPILE}
	${COMPILE.cc} ${COPTS.${.IMPSRC:T}} ${CPUFLAGS.${.IMPSRC:T}} ${CPPFLAGS.${.IMPSRC:T}} ${CSHLIBFLAGS} ${.IMPSRC} -o ${.TARGET}
.if !defined(CFLAGS) || empty(CFLAGS:M*-g*)
	${OBJCOPY} -x ${.TARGET}
.endif

.f.o:
	${_MKTARGET_COMPILE}
	${COMPILE.f} ${.IMPSRC} -o ${.TARGET}
.if !defined(FOPTS) || empty(FOPTS:M*-g*)
	${OBJCOPY} -x ${.TARGET}
.endif

.f.po:
	${_MKTARGET_COMPILE}
	${COMPILE.f} ${PROFFLAGS} -pg ${.IMPSRC} -o ${.TARGET}
.if !defined(FOPTS) || empty(FOPTS:M*-g*)
	${OBJCOPY} -X ${.TARGET}
.endif

.f.go:
	${_MKTARGET_COMPILE}
	${COMPILE.f} ${DEBUGFLAGS} -g ${.IMPSRC} -o ${.TARGET}

.f.so:
	${_MKTARGET_COMPILE}
	${COMPILE.f} ${FPICFLAGS} ${.IMPSRC} -o ${.TARGET}
.if !defined(FOPTS) || empty(FOPTS:M*-g*)
	${OBJCOPY} -x ${.TARGET}
.endif

.f.ln:
	${_MKTARGET_COMPILE}
	@echo Skipping lint for Fortran libraries.

.m.o:
	${_MKTARGET_COMPILE}
	${COMPILE.m} ${OBJCOPTS.${.IMPSRC:T}} ${.IMPSRC} -o ${.TARGET}
.if !defined(OBJCFLAGS) || empty(OBJCFLAGS:M*-g*)
	${OBJCOPY} -x ${.TARGET}
.endif

.m.po:
	${_MKTARGET_COMPILE}
	${COMPILE.m} ${PROFFLAGS} -pg ${OBJCOPTS.${.IMPSRC:T}} ${.IMPSRC} -o ${.TARGET}
.if !defined(OBJCFLAGS) || empty(OBJCFLAGS:M*-g*)
	${OBJCOPY} -X ${.TARGET}
.endif

.m.go:
	${_MKTARGET_COMPILE}
	${COMPILE.m} ${DEBUGFLAGS} -g ${OBJCOPTS.${.IMPSRC:T}} ${.IMPSRC} -o ${.TARGET}
.if !defined(OBJCFLAGS) || empty(OBJCFLAGS:M*-g*)
	${OBJCOPY} -X ${.TARGET}
.endif

.m.so:
	${_MKTARGET_COMPILE}
	${COMPILE.m} ${CSHLIBFLAGS} ${OBJCOPTS.${.IMPSRC:T}} ${.IMPSRC} -o ${.TARGET}
.if !defined(OBJCFLAGS) || empty(OBJCFLAGS:M*-g*)
	${OBJCOPY} -x ${.TARGET}
.endif

.s.o:
	${_MKTARGET_COMPILE}
	${COMPILE.s} ${COPTS.${.IMPSRC:T}} ${CPUFLAGS.${.IMPSRC:T}} ${CPPFLAGS.${.IMPSRC:T}} ${.IMPSRC} -o ${.TARGET}
	${OBJCOPY} -x ${.TARGET}

.S.o:
	${_MKTARGET_COMPILE}
	${COMPILE.S} ${COPTS.${.IMPSRC:T}} ${CPUFLAGS.${.IMPSRC:T}} ${CPPFLAGS.${.IMPSRC:T}} ${.IMPSRC} -o ${.TARGET}
	${OBJCOPY} -x ${.TARGET}

.s.po:
	${_MKTARGET_COMPILE}
	${COMPILE.s} ${PROFFLAGS} ${COPTS.${.IMPSRC:T}} ${CPUFLAGS.${.IMPSRC:T}} ${CPPFLAGS.${.IMPSRC:T}} ${.IMPSRC} -o ${.TARGET}
	${OBJCOPY} -X ${.TARGET}

.S.po:
	${_MKTARGET_COMPILE}
	${COMPILE.S} ${PROFFLAGS} ${COPTS.${.IMPSRC:T}} ${CPUFLAGS.${.IMPSRC:T}} ${CPPFLAGS.${.IMPSRC:T}} ${.IMPSRC} -o ${.TARGET}
	${OBJCOPY} -X ${.TARGET}

.s.go:
	${_MKTARGET_COMPILE}
	${COMPILE.s} ${DEBUGFLAGS} ${COPTS.${.IMPSRC:T}} ${CPUFLAGS.${.IMPSRC:T}} ${CPPFLAGS.${.IMPSRC:T}} ${.IMPSRC} -o ${.TARGET}

.S.go:
	${_MKTARGET_COMPILE}
	${COMPILE.S} ${DEBUGFLAGS} ${COPTS.${.IMPSRC:T}} ${CPUFLAGS.${.IMPSRC:T}} ${CPPFLAGS.${.IMPSRC:T}} ${.IMPSRC} -o ${.TARGET}

.s.so:
	${_MKTARGET_COMPILE}
	${COMPILE.s} ${CAPICFLAGS} ${COPTS.${.IMPSRC:T}} ${CPUFLAGS.${.IMPSRC:T}} ${CPPFLAGS.${.IMPSRC:T}} ${.IMPSRC} -o ${.TARGET}
	${OBJCOPY} -x ${.TARGET}

.S.so:
	${_MKTARGET_COMPILE}
	${COMPILE.S} ${CAPICFLAGS} ${COPTS.${.IMPSRC:T}} ${CPUFLAGS.${.IMPSRC:T}} ${CPPFLAGS.${.IMPSRC:T}} ${.IMPSRC} -o ${.TARGET}
	${OBJCOPY} -x ${.TARGET}

.if defined(LIB)							# {
.if (${MKPIC} == "no" || (defined(LDSTATIC) && ${LDSTATIC} != "") \
	|| ${MKLINKLIB} != "no") && ${MKSTATICLIB} != "no"
_LIBS=lib${LIB}.a
.else
_LIBS=
.endif

OBJS+=${SRCS:N*.h:N*.sh:R:S/$/.o/g}

STOBJS+=${OBJS}

LOBJS+=${LSRCS:.c=.ln} ${SRCS:M*.c:.c=.ln}

.if ${LIBISPRIVATE} != "no"
# No installation is required
libinstall::
.endif	# ${LIBISPRIVATE} == "no"					# {

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
SOBJS+=${OBJS:.o=.so}
.else
SOLIB=lib${LIB}.a
.endif
.else
SOLIB=lib${LIB}_pic.a
_LIBS+=${SOLIB}
SOBJS+=${OBJS:.o=.so}
.endif
.if defined(SHLIB_FULLVERSION)
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

.NOPATH: ${ALLOBJS} ${_LIBS} ${_YLSRCS}

realall: ${SRCS} ${ALLOBJS:O} ${_LIBS}

MKARZERO?=no

.if ${MKARZERO} == "yes"
_ARFL=crsD
_ARRANFL=sD
_INSTRANLIB=
.else
_ARFL=crs
_ARRANFL=s
#_INSTRANLIB=${empty(PRESERVE):?-a "${RANLIB} -t":}
.endif

# If you change this, please consider reflecting the change in
# the override in sys/rump/Makefile.rump.
.if !target(__archivebuild)
__archivebuild: .USE
	${_MKTARGET_BUILD}
	rm -f ${.TARGET}
#	${AR} ${_ARFL} ${.TARGET} `NM=${NM} ${LORDER} ${.ALLSRC:M*o} | ${TSORT}`
	${AR} ${_ARFL} ${.TARGET} ${.ALLSRC:M*o}
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

lib${LIB}.a:: ${STOBJS} __archivebuild

lib${LIB}_p.a:: ${POBJS} __archivebuild

lib${LIB}_pic.a:: ${SOBJS} __archivebuild

lib${LIB}_g.a:: ${GOBJS} __archivebuild


_LIBLDOPTS=
#.if ${SHLIBDIR} != "/usr/lib"
#_LIBLDOPTS+=	-Wl,-rpath-link,${DESTDIR}${SHLIBDIR}:${DESTDIR}/usr/lib \
#		-R${SHLIBDIR} \
#		-L${DESTDIR}${SHLIBDIR}
#.elif ${SHLIBINSTALLDIR} != "/usr/lib"
#_LIBLDOPTS+=	-Wl,-rpath-link,${DESTDIR}${SHLIBINSTALLDIR}:${DESTDIR}/usr/lib \
#		-L${DESTDIR}${SHLIBINSTALLDIR}
#.endif

# gcc -shared now adds -lc automatically. For libraries other than libc and
# libgcc* we add as a dependency the installed shared libc. For libc and
# libgcc* we avoid adding libc as a dependency by using -nostdlib. Note that
# -Xl,-nostdlib is not enough because we want to tell the compiler-driver not
# to add standard libraries, not the linker.
.if !defined(LIB)
DPLIBC ?= ${DESTDIR}${LIBC_SO}
.else
.if ${LIB} != "c" && ${LIB:Mgcc*} == ""
DPLIBC ?= ${DESTDIR}${LIBC_SO}
.else
LDLIBC ?= -nodefaultlibs
.if ${LIB} == "c"
#LDADD+= -lgcc_pic
.endif
.endif
.endif

.if ${LIBISCXX} != "no"
LIBCC:=	${CXX}
.else
LIBCC:=	${CC}
.endif

lib${LIB}.so.${SHLIB_FULLVERSION}: ${SOLIB} ${DPADD} ${DPLIBC} \
    ${SHLIB_LDSTARTFILE} ${SHLIB_LDENDFILE}
	${_MKTARGET_BUILD}
	rm -f lib${LIB}.so.${SHLIB_FULLVERSION}
.if defined(DESTDIR)
	${LIBCC} ${LDLIBC} -Wl,-nostdlib -B${_GCC_CRTDIR}/ -B${DESTDIR}/usr/lib/ \
	    -Wl,-x -shared ${SHLIB_SHFLAGS} -o ${.TARGET} \
	    -Wl,--whole-archive ${SOLIB} \
	    -Wl,--no-whole-archive ${LDADD} \
	    ${_LIBLDOPTS} ${LDFLAGS} \
	    -L${_GCC_LIBGCCDIR}
.else
	${LIBCC} ${LDLIBC} -Wl,-x -shared ${SHLIB_SHFLAGS} ${LDFLAGS} \
	    -o ${.TARGET} ${_LIBLDOPTS} \
	    -Wl,--whole-archive ${SOLIB} -Wl,--no-whole-archive ${LDADD}
.endif
.if ${OBJECT_FMT} == "ELF"
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
.endif
.if ${MKSTRIPIDENT} != "no"
	${OBJCOPY} -R .ident ${.TARGET}
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

cleanlib: .PHONY
	rm -f a.out [Ee]rrs mklog core *.core ${CLEANFILES}
	rm -f lib${LIB}.a ${STOBJS}
	rm -f lib${LIB}_p.a ${POBJS}
	rm -f lib${LIB}_g.a ${GOBJS}
	rm -f lib${LIB}_pic.a lib${LIB}.so.* lib${LIB}.so ${SOBJS}
	rm -f ${STOBJS:=.tmp} ${POBJS:=.tmp} ${SOBJS:=.tmp} ${GOBJS:=.tmp}
	rm -f llib-l${LIB}.ln ${LOBJS}


.if !target(libinstall)							# {
# Make sure it gets defined, in case MKPIC==no && MKLINKLIB==no
libinstall::

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
.if ${OBJECT_FMT} == "a.out" && !defined(DESTDIR)
	/sbin/ldconfig -m ${_LIBSODIR} ${LIBDIR}
.endif
.if ${OBJECT_FMT} == "ELF"
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
#.include <bsd.nls.mk>
.include <bsd.files.mk>
.include <bsd.inc.mk>
.include <bsd.links.mk>
.include <bsd.dep.mk>
.include <minix.gcc.mk>

${TARGETS}:	# ensure existence
