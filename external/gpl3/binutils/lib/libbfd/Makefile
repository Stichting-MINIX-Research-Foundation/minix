#	$NetBSD: Makefile,v 1.22 2020/04/04 00:46:53 christos Exp $

NOLINKLIB=	# defined
NOLINT=		# defined
NOMAN=		# defined
NOPROFILE=	# defined
NOCLANGERROR=	# defined

.include <bsd.init.mk>

# If the platform does not support shared libraries, we need to supply
# *something* for BFD-using programs to link against.  Also provide an
# empty libinstall target so that libbfd.a does not get installed.
.if ${MKPIC} == "no"
MKLINKLIB=	yes
libinstall:	# do nothing
.endif

LIB=		bfd

BFD_MACHINE_ARCH?=	${MACHINE_ARCH:C/armv[4-7]/arm/}

DEFS_MK=${.CURDIR}/arch/${BFD_MACHINE_ARCH}/defs.mk
# XXX: Fix me
BINDIR=/usr/bin
BINDIR=/usr/lib

.if exists(${DEFS_MK})
.include "${DEFS_MK}"

.if defined(__MINIX)
# MINIX: make sure sources are fetched, even when tools not built earlier
GNUHOSTDIST=	${DIST}
.include "${.CURDIR}/../../../../../minix/Makefile.fetchgnu"
.endif # defined(__MINIX)

SHLIB_MAJOR=	17
SHLIB_MINOR=	0

LIBDPLIBS+=	z	${.CURDIR}/../../../../../lib/libz

GCPPFLAGS=	${G_archdefs} ${G_DEFS} ${G_INCLUDES} ${G_TDEFAULTS}
GCPPFLAGS+=	${G_HAVEVECS}

CPPFLAGS+=	-I${.CURDIR}/arch/${BFD_MACHINE_ARCH} -I${DIST}/include -I. \
		-I${DIST}/bfd ${GCPPFLAGS:M-D*} ${GCPPFLAGS:M-I*:N-I.*} \
		-DDEBUGDIR=\"${DEBUGDIR}\" -DLIBDIR=\"${LIBDIR}\" \
		-DBINDIR=\"${BINDIR}\"

.if (${BFD_MACHINE_ARCH} == "vax")
CPPFLAGS.elf.c +=	-O0
.endif

COPTS.pei-x86_64.c += -Wno-stack-protector
COPTS.elfxx-mips.c += -Wno-stack-protector

GSRCS=		${G_libbfd_la_OBJECTS:libbfd.lo=lib_bfd.lo} \
		${G_libbfd_la_DEPENDENCIES:M*.lo}
SRCS=		${GSRCS:.lo=.c}

DPSRCS+=	elf32-target.h elf64-target.h targmatch.h \
		elf32-ia64.c elf64-ia64.c peigen.c pepigen.c \
		elf32-riscv.c elf64-riscv.c elf32-aarch64.c elf64-aarch64.c \
		pex64igen.c
CLEANFILES+=	elf32-target.h elf64-target.h targmatch.h \
		elf32-ia64.c elf64-ia64.c peigen.c pepigen.c \
		elf32-riscv.c elf64-riscv.c elf32-aarch64.c elf64-aarch64.c \
		pex64igen.c

TEXINFO=	bfd.texinfo
INFOFLAGS=	-I${DIST}/bfd/doc

.PATH: ${DIST}/bfd ${DIST}/bfd/doc

.include <bsd.lib.mk>
.include <bsd.info.mk>

${OBJS} ${SOBJS}: elf32-target.h elf64-target.h
targets.o targets.pico: targmatch.h Makefile
.else
.include <bsd.prog.mk> # do nothing
.endif

targmatch.h: config.bfd targmatch.sed
	${_MKTARGET_CREATE}
	${TOOL_SED} -f ${.ALLSRC:M*.sed} ${.ALLSRC:M*.bfd} >$@

elf32-target.h: elfxx-target.h
	${_MKTARGET_CREATE}
	${TOOL_SED} -e s/NN/32/g < $> > $@

elf64-target.h: elfxx-target.h
	${_MKTARGET_CREATE}
	${TOOL_SED} -e s/NN/64/g < $> > $@

elf32-ia64.c: elfnn-ia64.c
	${_MKTARGET_CREATE}
	${TOOL_SED} -e s/NN/32/g < $> > $@

elf64-ia64.c: elfnn-ia64.c
	${_MKTARGET_CREATE}
	${TOOL_SED} -e s/NN/64/g < $> > $@

elf32-riscv.c: elfnn-riscv.c
	${_MKTARGET_CREATE}
	${TOOL_SED} -e s/NN/32/g < $> > $@

elf64-riscv.c: elfnn-riscv.c
	${_MKTARGET_CREATE}
	${TOOL_SED} -e s/NN/64/g < $> > $@

elf32-aarch64.c: elfnn-aarch64.c
	${_MKTARGET_CREATE}
	${TOOL_SED} -e s/NN/32/g < $> > $@

elf64-aarch64.c: elfnn-aarch64.c
	${_MKTARGET_CREATE}
	${TOOL_SED} -e s/NN/64/g < $> > $@

peigen.c: peXXigen.c
	${_MKTARGET_CREATE}
	${TOOL_SED} -e s/XX/pe/g < $> > $@

pepigen.c: peXXigen.c
	${_MKTARGET_CREATE}
	${TOOL_SED} -e s/XX/pep/g < $> > $@

pex64igen.c: peXXigen.c
	${_MKTARGET_CREATE}
	${TOOL_SED} -e s/XX/pex64/g < $> > $@

.if defined(__MINIX) && ${USETOOLS} != "yes"
# Trigger the fetch phase, even when not building tools
${SRCS} realdepend realall realinstall: ${fetch_done}
.endif # defined(__MINIX)
