#	$NetBSD: bsd.sys.mk,v 1.245 2014/09/03 19:22:53 matt Exp $
#
# Build definitions used for NetBSD source tree builds.

.if !defined(_BSD_SYS_MK_)
_BSD_SYS_MK_=1

.if !empty(.INCLUDEDFROMFILE:MMakefile*)
error1:
	@(echo "bsd.sys.mk should not be included from Makefiles" >& 2; exit 1)
.endif
.if !defined(_BSD_OWN_MK_)
error2:
	@(echo "bsd.own.mk must be included before bsd.sys.mk" >& 2; exit 1)
.endif

.if defined(__MINIX)
#LSC: Be a bit smarter about the default compiler
.if exists(/usr/pkg/bin/clang) || exists(/usr/bin/clang)
CC?=	clang
.endif

.if exists(/usr/pkg/bin/gcc) || exists(/usr/bin/gcc)
CC?=	gcc
.endif

MKDEP?= CC=${CC:Q} mkdep
MKDEPCXX?= CC=${CXX:Q} mkdep
.endif # defined(__MINIX)

.if ${MKREPRO:Uno} == "yes"
CPPFLAGS+=	-Wp,-iremap,${NETBSDSRCDIR}:/usr/src
CPPFLAGS+=	-Wp,-iremap,${DESTDIR}/:/
CPPFLAGS+=	-Wp,-iremap,${X11SRCDIR}:/usr/xsrc
.endif

# NetBSD sources use C99 style, with some GCC extensions.
CFLAGS+=	${${ACTIVE_CC} == "clang":? -std=gnu99 :}
CFLAGS+=	${${ACTIVE_CC} == "gcc":? -std=gnu99 :}
CFLAGS+=	${${ACTIVE_CC} == "pcc":? -std=gnu99 :}

.if defined(WARNS)
CFLAGS+=	${${ACTIVE_CC} == "clang":? -Wno-sign-compare -Wno-pointer-sign :}
.if ${WARNS} > 0
CFLAGS+=	-Wall -Wstrict-prototypes -Wmissing-prototypes -Wpointer-arith
#CFLAGS+=	-Wmissing-declarations -Wredundant-decls -Wnested-externs
# Add -Wno-sign-compare.  -Wsign-compare is included in -Wall as of GCC 3.3,
# but our sources aren't up for it yet. Also, add -Wno-traditional because
# gcc includes #elif in the warnings, which is 'this code will not compile
# in a traditional environment' warning, as opposed to 'this code behaves
# differently in traditional and ansi environments' which is the warning
# we wanted, and now we don't get anymore.
CFLAGS+=	-Wno-sign-compare
CFLAGS+=	${${ACTIVE_CC} == "gcc" :? -Wno-traditional :}
.if !defined(NOGCCERROR) && !defined(USE_BITCODE)
# Set assembler warnings to be fatal
CFLAGS+=	-Wa,--fatal-warnings
.endif
# Set linker warnings to be fatal
# XXX no proper way to avoid "FOO is a patented algorithm" warnings
# XXX on linking static libs
.if (!defined(MKPIC) || ${MKPIC} != "no") && \
    (!defined(LDSTATIC) || ${LDSTATIC} != "-static")
# XXX there are some strange problems not yet resolved
. if defined(HAVE_LLVM) && !defined(USE_BITCODE)
LDFLAGS+=	-Wl,--fatal-warnings
. endif
.endif
.endif
.if ${WARNS} > 1
CFLAGS+=	-Wreturn-type -Wswitch -Wshadow
.endif
.if ${WARNS} > 2
CFLAGS+=	-Wcast-qual -Wwrite-strings
CFLAGS+=	-Wextra -Wno-unused-parameter
# Readd -Wno-sign-compare to override -Wextra with clang
CFLAGS+=	-Wno-sign-compare
CXXFLAGS+=	-Wabi
CXXFLAGS+=	-Wold-style-cast
CXXFLAGS+=	-Wctor-dtor-privacy -Wnon-virtual-dtor -Wreorder \
		-Wno-deprecated -Woverloaded-virtual -Wsign-promo -Wsynth
CXXFLAGS+=	${${ACTIVE_CXX} == "gcc":? -Wno-non-template-friend -Wno-pmf-conversions :}
.endif
.if ${WARNS} > 3 && (defined(HAVE_GCC) || defined(HAVE_LLVM))
.if ${WARNS} > 4
CFLAGS+=	-Wold-style-definition
.endif
.if ${WARNS} > 5 && !(defined(HAVE_GCC) && ${HAVE_GCC} <= 45)
CFLAGS+=	-Wconversion
.endif
CFLAGS+=	-Wsign-compare -Wformat=2
CFLAGS+=	${${ACTIVE_CC} == "gcc":? -Wno-format-zero-length :}
.endif
.if ${WARNS} > 3 && defined(HAVE_LLVM)
CFLAGS+=	${${ACTIVE_CC} == "clang":? -Wpointer-sign -Wmissing-noreturn :}
.endif
.if (defined(HAVE_GCC) \
     && (${MACHINE_ARCH} == "coldfire" || \
	 ${MACHINE_ARCH} == "sh3eb" || \
	 ${MACHINE_ARCH} == "sh3el" || \
	 ${MACHINE_ARCH} == "m68k" || \
	 ${MACHINE_ARCH} == "m68000"))
# XXX GCC 4.5 for sh3 and m68k (which we compile with -Os) is extra noisy for
# cases it should be better with
CFLAGS+=	-Wno-uninitialized
.if ${HAVE_GCC} >= 48
CFLAGS+=	-Wno-maybe-uninitialized
.endif
.endif
.endif

CWARNFLAGS+=	${CWARNFLAGS.${ACTIVE_CC}}

CPPFLAGS+=	${AUDIT:D-D__AUDIT__}
_NOWERROR=	${defined(NOGCCERROR) || (${ACTIVE_CC} == "clang" && defined(NOCLANGERROR)):?yes:no}
.if defined(__MINIX) && ${USE_BITCODE:Uno} == "yes"
_NOWERROR=	yes
.endif # defined(__MINIX) && ${USE_BITCODE} == "yes"
CFLAGS+=	${${_NOWERROR} == "no" :?-Werror:} ${CWARNFLAGS}
LINTFLAGS+=	${DESTDIR:D-d ${DESTDIR}/usr/include}

.if (${USE_SSP:Uno} != "no") && (${BINDIR:Ux} != "/usr/mdec")
.if !defined(KERNSRCDIR) && !defined(KERN) # not for kernels nor kern modules
CPPFLAGS+=	-D_FORTIFY_SOURCE=2
.endif
COPTS+=	-fstack-protector -Wstack-protector 

# gcc 4.8 on m68k erroneously does not protect functions with
# variables needing special alignement, see
#	http://gcc.gnu.org/bugzilla/show_bug.cgi?id=59674
# (the underlying issue for sh and vax may be different, needs more
# investigation, symptoms are similar but for different sources)
.if "${ACTIVE_CC}" == "gcc" && "${HAVE_GCC}" == "48" && \
	( ${MACHINE_CPU} == "sh3" || \
	  ${MACHINE_ARCH} == "vax" || \
	  ${MACHINE_CPU} == "m68k" || \
	  ${MACHINE_CPU} == "or1k" )
COPTS+=	-Wno-error=stack-protector 
.endif

COPTS+=	${${ACTIVE_CC} == "clang":? --param ssp-buffer-size=1 :}
COPTS+=	${${ACTIVE_CC} == "gcc":? --param ssp-buffer-size=1 :}
.endif

.if ${MKSOFTFLOAT:Uno} != "no"
COPTS+=		${${ACTIVE_CC} == "gcc":? -msoft-float :}
FOPTS+=		-msoft-float
.elif ${MACHINE_ARCH} == "coldfire"
COPTS+=		-mhard-float
FOPTS+=		-mhard-float
.endif

.if ${MKIEEEFP:Uno} != "no"
.if ${MACHINE_ARCH} == "alpha"
CFLAGS+=	-mieee
FFLAGS+=	-mieee
.endif
.endif

.if ${MACHINE} == "sparc64" && ${MACHINE_ARCH} == "sparc"
CFLAGS+=	-Wa,-Av8plus
.endif

.if !defined(NOGCCERROR)
.if (${MACHINE_ARCH} == "mips64el") || (${MACHINE_ARCH} == "mips64eb")
CPUFLAGS+=	-Wa,--fatal-warnings
.endif
.endif

#.if ${MACHINE} == "sbmips"
#CFLAGS+=	-mips64 -mtune=sb1
#.endif

#.if (${MACHINE_ARCH} == "mips64el" || ${MACHINE_ARCH} == "mips64eb") && \
#    (defined(MKPIC) && ${MKPIC} == "no")
#CPUFLAGS+=	-mno-abicalls -fno-PIC
#.endif
CFLAGS+=	${CPUFLAGS}
AFLAGS+=	${CPUFLAGS}

.if !defined(LDSTATIC) || ${LDSTATIC} != "-static"
# Position Independent Executable flags
PIE_CFLAGS?=        -fPIC
PIE_LDFLAGS?=       -Wl,-pie ${${ACTIVE_CC} == "gcc":? -shared-libgcc :}
PIE_AFLAGS?=	    -fPIC
.endif

ELF2ECOFF?=	elf2ecoff
MKDEP?=		mkdep
MKDEPCXX?=	mkdep
OBJCOPY?=	objcopy
OBJDUMP?=	objdump
PAXCTL?=	paxctl
STRIP?=		strip

.SUFFIXES:	.o .ln .lo .c .cc .cpp .cxx .C .m ${YHEADER:D.h}

# C
.c.o:
	${_MKTARGET_COMPILE}
	${COMPILE.c} ${COPTS.${.IMPSRC:T}} ${CPUFLAGS.${.IMPSRC:T}} ${CPPFLAGS.${.IMPSRC:T}} ${.IMPSRC}
.if defined(CTFCONVERT)
	${CTFCONVERT} ${CTFFLAGS} ${.TARGET}
.endif

.c.ln:
	${_MKTARGET_COMPILE}
	${LINT} ${LINTFLAGS} ${LINTFLAGS.${.IMPSRC:T}} \
	    ${CPPFLAGS:C/-([IDU])[  ]*/-\1/Wg:M-[IDU]*} \
	    ${CPPFLAGS.${.IMPSRC:T}:C/-([IDU])[  ]*/-\1/Wg:M-[IDU]*} \
	    -i ${.IMPSRC}

# C++
.cc.o .cpp.o .cxx.o .C.o:
	${_MKTARGET_COMPILE}
	${COMPILE.cc} ${COPTS.${.IMPSRC:T}} ${CPUFLAGS.${.IMPSRC:T}} ${CPPFLAGS.${.IMPSRC:T}} ${.IMPSRC}

# Objective C
# (Defined here rather than in <sys.mk> because `.m' is not just
#  used for Objective C source)
.m.o:
	${_MKTARGET_COMPILE}
	${COMPILE.m} ${OBJCOPTS} ${OBJCOPTS.${.IMPSRC:T}} ${.IMPSRC}
.if defined(CTFCONVERT)
	${CTFCONVERT} ${CTFFLAGS} ${.TARGET}
.endif

# Host-compiled C objects
# The intermediate step is necessary for Sun CC, which objects to calling
# object files anything but *.o
.c.lo:
	${_MKTARGET_COMPILE}
	${HOST_COMPILE.c} -o ${.TARGET}.o ${COPTS.${.IMPSRC:T}} ${CPUFLAGS.${.IMPSRC:T}} ${CPPFLAGS.${.IMPSRC:T}} ${.IMPSRC}
	mv ${.TARGET}.o ${.TARGET}

# C++
.cc.lo .cpp.lo .cxx.lo .C.lo:
	${_MKTARGET_COMPILE}
	${HOST_COMPILE.cc} -o ${.TARGET}.o ${COPTS.${.IMPSRC:T}} ${CPUFLAGS.${.IMPSRC:T}} ${CPPFLAGS.${.IMPSRC:T}} ${.IMPSRC}
	mv ${.TARGET}.o ${.TARGET}

# Assembly
.s.o:
	${_MKTARGET_COMPILE}
	${COMPILE.s} ${COPTS.${.IMPSRC:T}} ${CPUFLAGS.${.IMPSRC:T}} ${CPPFLAGS.${.IMPSRC:T}} ${.IMPSRC}
.if defined(CTFCONVERT)
	${CTFCONVERT} ${CTFFLAGS} ${.TARGET}
.endif

.S.o:
	${_MKTARGET_COMPILE}
	${COMPILE.S} ${COPTS.${.IMPSRC:T}} ${CPUFLAGS.${.IMPSRC:T}} ${CPPFLAGS.${.IMPSRC:T}} ${.IMPSRC}
.if defined(CTFCONVERT)
	${CTFCONVERT} ${CTFFLAGS} ${.TARGET}
.endif

# Lex
LFLAGS+=	${LPREFIX.${.IMPSRC:T}:D-P${LPREFIX.${.IMPSRC:T}}}
LFLAGS+=	${LPREFIX:D-P${LPREFIX}}

.l.c:
	${_MKTARGET_LEX}
	${LEX.l} -o${.TARGET} ${.IMPSRC}

# Yacc
YFLAGS+=	${YPREFIX.${.IMPSRC:T}:D-p${YPREFIX.${.IMPSRC:T}}} ${YHEADER.${.IMPSRC:T}:D-d}
YFLAGS+=	${YPREFIX:D-p${YPREFIX}} ${YHEADER:D-d}

.y.c:
	${_MKTARGET_YACC}
	${YACC.y} -o ${.TARGET} ${.IMPSRC}

.ifdef YHEADER
.if empty(.MAKEFLAGS:M-n)
.y.h: ${.TARGET:.h=.c}
.endif
.endif

# Objcopy
.if ${MACHINE_ARCH} == aarch64eb
# AARCH64 big endian needs to preserve $x/$d symbols for the linker.
OBJCOPYLIBFLAGS_EXTRA=-w -K '[$$][dx]' -K '[$$][dx]\.*'
.elif !empty(MACHINE_ARCH:M*arm*eb)
# ARM big endian needs to preserve $a/$d/$t symbols for the linker.
OBJCOPYLIBFLAGS_EXTRA=-w -K '[$$][adt]' -K '[$$][adt]\.*'
.endif
OBJCOPYLIBFLAGS?=${"${.TARGET:M*.po}" != "":?-X:-x} ${OBJCOPYLIBFLAGS_EXTRA}

.endif	# !defined(_BSD_SYS_MK_)
