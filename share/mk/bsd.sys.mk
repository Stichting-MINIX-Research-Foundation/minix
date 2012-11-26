#	$NetBSD: bsd.sys.mk,v 1.220 2012/09/23 19:20:44 joerg Exp $
#
# Build definitions used for NetBSD source tree builds.

.if !defined(_BSD_SYS_MK_)
_BSD_SYS_MK_=1

.if ${HOST_OSTYPE:C/\-.*//:U} == "Minix"
HOST_CPP?=	/usr/lib/cpp
HOST_LDFLAGS?=	-static
.endif

.if ${MKREPRO:Uno} == "yes"
CPPFLAGS+=	-Wp,-iremap,${NETBSDSRCDIR}:/usr/src
CPPFLAGS+=	-Wp,-iremap,${DESTDIR}/:/
CPPFLAGS+=	-Wp,-iremap,${X11SRCDIR}:/usr/xsrc
.endif

# Enable c99 mode by default.
# This has the side effect of complaining for missing prototypes
# implicit type declarations and missing return statements.
.if defined(HAVE_GCC) || defined(HAVE_LLVM)
CFLAGS+=	-std=gnu99
.endif

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
CFLAGS+=	${${ACTIVE_CC} != "clang":? -Wno-traditional :}
.if !defined(NOGCCERROR)
# Set assembler warnings to be fatal
#CFLAGS+=	-Wa,--fatal-warnings
# LSC Clang version 2.9 those not support this flag
CFLAGS+=       ${${HAVE_LLVM:U"0.0"} != "2.9":? -Wa,--fatal-warnings:}
.endif
# Set linker warnings to be fatal
# XXX no proper way to avoid "FOO is a patented algorithm" warnings
# XXX on linking static libs
.if (!defined(MKPIC) || ${MKPIC} != "no") && \
    (!defined(LDSTATIC) || ${LDSTATIC} != "-static")
# XXX there are some strange problems not yet resolved
. if !defined(HAVE_GCC) || defined(HAVE_LLVM)
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
CFLAGS+=	-Wsign-compare -Wformat=2
CFLAGS+=	${${ACTIVE_CC} == "clang":? -Wno-error=format-nonliteral :}
CFLAGS+=	${${ACTIVE_CC} == "gcc":? -Wno-format-zero-length :}
.endif
.if ${WARNS} > 3 && defined(HAVE_LLVM)
CFLAGS+=	${${ACTIVE_CC} == "clang":? -Wpointer-sign -Wmissing-noreturn :}
.endif
.if (defined(HAVE_GCC) && ${HAVE_GCC} == 45 \
     && (${MACHINE_ARCH} == "sh3eb" || \
	 ${MACHINE_ARCH} == "sh3el" || \
	 ${MACHINE_ARCH} == "m68k" || \
	 ${MACHINE_ARCH} == "m68000"))
# XXX GCC 4.5 for sh3 and m68k (which we compile with -Os) is extra noisy for
# cases it should be better with
CFLAGS+=	-Wno-uninitialized
.endif
.endif

CWARNFLAGS+=	${CWARNFLAGS.${ACTIVE_CC}}

CPPFLAGS+=	${AUDIT:D-D__AUDIT__}
_NOWERROR=	${defined(NOGCCERROR) || (${ACTIVE_CC} == "clang" && defined(NOCLANGERROR)):?yes:no}
CFLAGS+=	${${_NOWERROR} == "no" :?-Werror:} ${CWARNFLAGS}
LINTFLAGS+=	${DESTDIR:D-d ${DESTDIR}/usr/include}

.if (${MACHINE_ARCH} == "alpha") || \
    (${MACHINE_ARCH} == "hppa") || \
    (${MACHINE_ARCH} == "ia64") || \
    (${MACHINE_ARCH} == "mipsel") || (${MACHINE_ARCH} == "mipseb") || \
    (${MACHINE_ARCH} == "mips64el") || (${MACHINE_ARCH} == "mips64eb")
HAS_SSP=	no
.else
HAS_SSP=	yes
.endif

.if ${USE_FORT:Uno} != "no"
USE_SSP?=	yes
.if !defined(KERNSRCDIR) && !defined(KERN) # not for kernels nor kern modules
CPPFLAGS+=	-D_FORTIFY_SOURCE=2
.endif
.endif

.if (${USE_SSP:Uno} != "no") && (${BINDIR:Ux} != "/usr/mdec")
.if ${HAS_SSP} == "yes"
COPTS+=	-fstack-protector -Wstack-protector 
.if defined(__MINIX)
COPTS+=	${${ACTIVE_CC} == "clang":? -mllvm -stack-protector-buffer-size=1 :}
.else
COPTS+=	${${ACTIVE_CC} == "clang":? --param ssp-buffer-size=1 :}
.endif # defined(__MINIX)
COPTS+=	${${ACTIVE_CC} == "gcc":? --param ssp-buffer-size=1 :}
.endif
.endif

.if ${MKSOFTFLOAT:Uno} != "no"
COPTS+=		-msoft-float
FOPTS+=		-msoft-float
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
PIE_CFLAGS?=        -fPIC -DPIC
PIE_LDFLAGS?=       -Wl,-pie -shared-libgcc
PIE_AFLAGS?=	    -fPIC -DPIC
.endif

# Helpers for cross-compiling
HOST_CC?=	cc
HOST_CFLAGS?=	-O
HOST_COMPILE.c?=${HOST_CC} ${HOST_CFLAGS} ${HOST_CPPFLAGS} -c
HOST_COMPILE.cc?=      ${HOST_CXX} ${HOST_CXXFLAGS} ${HOST_CPPFLAGS} -c
.if defined(HOSTPROG_CXX) 
HOST_LINK.c?=	${HOST_CXX} ${HOST_CXXFLAGS} ${HOST_CPPFLAGS} ${HOST_LDFLAGS}
.else
HOST_LINK.c?=	${HOST_CC} ${HOST_CFLAGS} ${HOST_CPPFLAGS} ${HOST_LDFLAGS}
.endif

HOST_CXX?=	c++
HOST_CXXFLAGS?=	-O

HOST_CPP?=	cpp
HOST_CPPFLAGS?=

HOST_LD?=	ld
HOST_LDFLAGS?=

HOST_AR?=	ar
HOST_RANLIB?=	ranlib

HOST_LN?=	ln

# HOST_SH must be an absolute path
HOST_SH?=	/bin/sh

ELF2ECOFF?=	elf2ecoff
MKDEP?=		mkdep
OBJCOPY?=	objcopy
OBJDUMP?=	objdump
PAXCTL?=	paxctl
STRIP?=		strip

# TOOL_* variables are defined in bsd.own.mk

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
OBJCOPYLIBFLAGS?=${"${.TARGET:M*.po}" != "":?-X:-x}

.endif	# !defined(_BSD_SYS_MK_)
