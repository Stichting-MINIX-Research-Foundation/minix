#	$NetBSD: bsd.sys.mk,v 1.186 2009/11/30 16:13:23 uebayasi Exp $
#
# Build definitions used for NetBSD source tree builds.

.if !defined(_MINIX_SYS_MK_)
_MINIX_SYS_MK_=1

.if defined(WARNS)
.if ${WARNS} > 0
CFLAGS+=	-Wall -Wstrict-prototypes -Wmissing-prototypes -Wpointer-arith
#CFLAGS+=	-Wmissing-declarations -Wredundant-decls -Wnested-externs
# Add -Wno-sign-compare.  -Wsign-compare is included in -Wall as of GCC 3.3,
# but our sources aren't up for it yet. Also, add -Wno-traditional because
# gcc includes #elif in the warnings, which is 'this code will not compile
# in a traditional environment' warning, as opposed to 'this code behaves
# differently in traditional and ansi environments' which is the warning
# we wanted, and now we don't get anymore.
CFLAGS+=	-Wno-sign-compare -Wno-traditional
.if !defined(NOGCCERROR)
# Set assembler warnings to be fatal
CFLAGS+=	-Wa,--fatal-warnings
.endif
# Set linker warnings to be fatal
# XXX no proper way to avoid "FOO is a patented algorithm" warnings
# XXX on linking static libs
.if (!defined(MKPIC) || ${MKPIC} != "no") && \
    (!defined(LDSTATIC) || ${LDSTATIC} != "-static")
LDFLAGS+=	-Wl,--fatal-warnings
.endif
.endif
.if ${WARNS} > 1
CFLAGS+=	-Wreturn-type -Wswitch -Wshadow
.endif
.if ${WARNS} > 2
CFLAGS+=	-Wcast-qual -Wwrite-strings
CFLAGS+=	-Wextra -Wno-unused-parameter
CXXFLAGS+=	-Wabi
CXXFLAGS+=	-Wold-style-cast
CXXFLAGS+=	-Wctor-dtor-privacy -Wnon-virtual-dtor -Wreorder \
		-Wno-deprecated -Wno-non-template-friend \
		-Woverloaded-virtual -Wno-pmf-conversions -Wsign-promo -Wsynth
.endif
.if ${WARNS} > 3 && defined(HAVE_GCC) && ${HAVE_GCC} >= 3
CFLAGS+=	-Wsign-compare
CFLAGS+=	-std=gnu99
.endif
.endif

# XXX: Temporarily disabled for MINIX
# CPPFLAGS+=	${AUDIT:D-D__AUDIT__}
# CFLAGS+=	${CWARNFLAGS} ${NOGCCERROR:D:U-Werror}
# LINTFLAGS+=	${DESTDIR:D-d ${DESTDIR}/usr/include}

.if (${MACHINE_ARCH} == "alpha") || \
    (${MACHINE_ARCH} == "hppa") || \
    (${MACHINE_ARCH} == "ia64") || \
    (${MACHINE_ARCH} == "mipsel") || (${MACHINE_ARCH} == "mipseb") || \
    (${MACHINE_ARCH} == "mips64el") || (${MACHINE_ARCH} == "mips64eb")
HAS_SSP=	no
.else
HAS_SSP=	yes
.endif

.if defined(USE_FORT) && (${USE_FORT} != "no")
USE_SSP?=	yes
.if !defined(KERNSRCDIR) && !defined(KERN) # not for kernels nor kern modules
CPPFLAGS+=	-D_FORTIFY_SOURCE=2
.endif
.endif

.if defined(USE_SSP) && (${USE_SSP} != "no") && (${BINDIR:Ux} != "/usr/mdec")
.if ${HAS_SSP} == "yes"
COPTS+=		-fstack-protector -Wstack-protector --param ssp-buffer-size=1
.endif
.endif

.if defined(MKSOFTFLOAT) && (${MKSOFTFLOAT} != "no")
COPTS+=		-msoft-float
FOPTS+=		-msoft-float
.endif

.if defined(MKIEEEFP) && (${MKIEEEFP} != "no")
.if ${MACHINE_ARCH} == "alpha"
CFLAGS+=	-mieee
FFLAGS+=	-mieee
.endif
.endif

.if ${MACHINE} == "sparc64" && ${MACHINE_ARCH} == "sparc"
CFLAGS+=	-Wa,-Av8plus
.endif

CFLAGS+=	${CPUFLAGS}
AFLAGS+=	${CPUFLAGS}

# Position Independent Executable flags
PIE_CFLAGS?=        -fPIC -DPIC
PIE_LDFLAGS?=       -Wl,-pie -shared-libgcc
PIE_AFLAGS?=	    -fPIC -DPIC

# Helpers for cross-compiling
HOST_CC?=	cc
HOST_CFLAGS?=	-O
HOST_CFLAGS?=
HOST_COMPILE.c?=${HOST_CC} ${HOST_CFLAGS} ${HOST_CPPFLAGS} -c
HOST_COMPILE.cc?=      ${HOST_CXX} ${HOST_CXXFLAGS} ${HOST_CPPFLAGS} -c
.if defined(HOSTPROG_CXX) 
HOST_LINK.c?=	${HOST_CXX} ${HOST_CXXFLAGS} ${HOST_CPPFLAGS} ${HOST_LDFLAGS}
.else
HOST_LINK.c?=	${HOST_CC} ${HOST_CFLAGS} ${HOST_CPPFLAGS} ${HOST_LDFLAGS}
.endif

HOST_CXX?=	c++
HOST_CXXFLAGS?=	-O
HOST_CXXFLAGS?=

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

.c.ln:
	${_MKTARGET_COMPILE}
	${LINT} ${LINTFLAGS} \
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

.S.o:
	${_MKTARGET_COMPILE}
	${COMPILE.S} ${COPTS.${.IMPSRC:T}} ${CPUFLAGS.${.IMPSRC:T}} ${CPPFLAGS.${.IMPSRC:T}} ${.IMPSRC}

# Lex
LFLAGS+=	${LPREFIX.${.IMPSRC:T}:D-P${LPREFIX.${.IMPSRC:T}}}
LFLAGS+=	${LPREFIX:D-P${LPREFIX}}

.l.c:
	${_MKTARGET_LEX}
	${LEX.l} -o${.TARGET} ${.IMPSRC}

# Yacc
YFLAGS+=	${YPREFIX.${.IMPSRC:T}:D-p${YPREFIX.${.IMPSRC:T}}} ${YHEADER.${.IMPSRC:T}:D-d}
YFLAGS+=	${YPREFIX:D-p${YPREFIX}} ${YHEADER:D-d}

.ifdef QUIET_YACC
.y.c:
	${_MKTARGET_YACC}
	${YACC.y} -o ${.TARGET} ${.IMPSRC} 2> /dev/null
.else
.y.c:
	${_MKTARGET_YACC}
	${YACC.y} -o ${.TARGET} ${.IMPSRC}
.endif

.ifdef YHEADER
.y.h: ${.TARGET:.h=.c}
.endif

.endif	# !defined(_MINIX_SYS_MK_)
