#	$NetBSD: Makefile,v 1.40 2009/12/13 05:01:33 nakayama Exp $

# For now, we install the machine and arch includes, and symlink 'machine'
# to the location of the machine includes (usually).
#
# Eventually, we should install everything.

.include <bsd.own.mk>

SUBDIR=	${MACHINE}
ARCHSUBDIR= ${MACHINE_CPU}

.if ${ARCHSUBDIR} == "mips64"
ARCHSUBDIR= mips
.endif
.if ${ARCHSUBDIR} == "powerpc64"
ARCHSUBDIR= powerpc
.endif

.if ${MACHINE} != ${ARCHSUBDIR}
.if exists(${ARCHSUBDIR})
SUBDIR+= ${ARCHSUBDIR}
.endif
.endif
.if ${MACHINE} == sparc
SUBDIR+= sparc64
.endif
.if (${MACHINE} == hpcmips || ${MACHINE} == hpcsh)
SUBDIR+= hpc
.endif
.if (${MACHINE} == sun2 || ${MACHINE} == sun3)
SUBDIR+= sun68k
.endif
.if defined(XEN_BUILD)
SUBDIR+= xen
.endif


INCSYMLINKS= ${MACHINE_ARCH} /usr/include/machine

INCSYMLINKS+= machine/float.h /usr/include/float.h \
       machine/stdarg.h /usr/include/stdarg.h


.include <bsd.inc.mk>
.include <bsd.subdir.mk>
