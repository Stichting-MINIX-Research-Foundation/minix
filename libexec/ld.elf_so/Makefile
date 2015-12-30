#	$NetBSD: Makefile,v 1.129 2014/09/19 17:43:33 matt Exp $
#
# NOTE: when changing ld.so, ensure that ldd still compiles.
#
#MINIX:
USE_BITCODE=no

WARNS?=4

LDELFSO_MACHINE_ARCH?= ${MACHINE_ARCH}

# This needs to be before bsd.init.mk
.if defined(BSD_MK_COMPAT_FILE)
.include <${BSD_MK_COMPAT_FILE}>
.endif

# We are not building this with PIE
PIE_CFLAGS=
PIE_LDFLAGS=
.include <bsd.init.mk>			# for MKPIC definition
.include <bsd.shlib.mk>			# for SHLINKINSTALLDIR definition

.if defined(LDELFSO_MACHINE_CPU) && !empty(LDELFSO_MACHINE_CPU) && \
    exists(${.CURDIR}/arch/${LDELFSO_MACHINE_CPU})
ARCHSUBDIR=	${LDELFSO_MACHINE_CPU}
.else
ARCHSUBDIR=	${MACHINE_CPU}
.endif
M=		${.CURDIR}/arch/${ARCHSUBDIR}

.if ((${MACHINE_CPU} == "aarch64") ||					\
     (${LDELFSO_MACHINE_ARCH} == "alpha") ||				\
     (${MACHINE_CPU} == "arm") ||					\
     (${LDELFSO_MACHINE_ARCH} == "coldfire") ||				\
     (${LDELFSO_MACHINE_ARCH} == "hppa") ||				\
     (${LDELFSO_MACHINE_ARCH} == "i386") ||				\
     (${LDELFSO_MACHINE_ARCH} == "m68k") ||				\
     (${MACHINE_CPU} == "mips") ||					\
     (${MACHINE_CPU} == "or1k") ||					\
     (${MACHINE_CPU} == "powerpc") ||					\
     (${MACHINE_CPU} == "riscv") ||					\
     (${MACHINE_CPU} == "sh3") ||					\
     (${LDELFSO_MACHINE_ARCH} == "sparc") ||				\
     (${LDELFSO_MACHINE_ARCH} == "sparc64") ||				\
     (${LDELFSO_MACHINE_ARCH} == "x86_64") ||				\
     (${LDELFSO_MACHINE_ARCH} == "vax")) &&				\
    ${MKPIC} != "no"

LDFLAGS+=	${${ACTIVE_CC} == "clang":? -Wl,-Bsymbolic : -symbolic} \
		-shared -nostartfiles -nodefaultlibs
.if defined(__MINIX) && ${HAVE_GOLD:Uno} == "no"
LDFLAGS+=	-Wl,-static
.endif
LDFLAGS+=	-Wl,--warn-shared-textrel
LDFLAGS+=	-Wl,-Map=${.TARGET}.map

COPTS+=		-fvisibility=hidden

# Adds SRCS, CPPFLAGS, LDFLAGS, etc.  Must go first so MD startup source
# is first.
.if exists($M/Makefile.inc)
.include "$M/Makefile.inc"
.endif

# Support compat ld.elf_so.
.if defined(MLIBDIR)
PROG=		ld.elf_so-${MLIBDIR}
CPPFLAGS+=	-DRTLD_ARCH_SUBDIR=\"${MLIBDIR}\"
.else
PROG=		ld.elf_so
.endif

CLIBOBJ!=	cd ${NETBSDSRCDIR}/lib/libc && ${PRINTOBJDIR}

SRCS+=		rtld.c reloc.c symbol.c xmalloc.c xprintf.c debug.c \
		map_object.c load.c search.c headers.c paths.c expand.c \
		tls.c symver.c diagassert.c

.if ${USE_FORT} == "yes"
.PATH.c: ${NETBSDSRCDIR}/lib/libc/misc
SRCS+=		stack_protector.c
.endif

.PATH.c: ${NETBSDSRCDIR}/lib/libc/stdlib
SRCS+=		exit.c

errlist_concat.h: ${NETBSDSRCDIR}/lib/libc/gen/errlist.awk ${NETBSDSRCDIR}/sys/sys/errno.h
	${TOOL_AWK} -v concat=1 -f ${.ALLSRC} > ${.TARGET}.tmp && \
	mv -f ${.TARGET}.tmp ${.TARGET}

xprintf.c: errlist_concat.h

CLEANFILES+=	errlist_concat.h ${PROG}.map

BINDIR=		${SHLINKINSTALLDIR}

CPPFLAGS+=	-DLIBDIR=\"${LIBDIR}\" -D_PATH_RTLD=\"${BINDIR}/${PROG}\"
CPPFLAGS+=	-I${.CURDIR} -I.
CPPFLAGS+=	-DRTLD_LOADER
CPPFLAGS+=	-D_RTLD_SOURCE
CPPFLAGS+=	-DCOMBRELOC
#CPPFLAGS+=	-DDEBUG
#CPPFLAGS+=	-DRTLD_DEBUG
#CPPFLAGS+=	-DRTLD_DEBUG_RELOC
.if !empty(LDELFSO_MACHINE_ARCH:Mearm*)
CPPFLAGS+=	-I${NETBSDSRCDIR}/lib/libexecinfo
.endif
#DBG=		-g
COPTS+=		-O3

.if ${MACHINE_CPU} != "m68k" && ${MACHINE_CPU} != "sh3" && ${MACHINE_ARCH} != "vax"
COPTS+=		-fomit-frame-pointer
.endif

.if ${LDELFSO_MACHINE_ARCH} == "i386" || ${LDELFSO_MACHINE_ARCH} == "x86_64"
COPTS+=		-mno-3dnow -mno-mmx -mno-sse -mno-sse2 -mno-sse3
.endif


.if ${SHLIBDIR} != ${LIBDIR}
CPPFLAGS+=	-DRTLD_DEFAULT_LIBRARY_PATH=\"${SHLIBDIR}:${LIBDIR}\"
.endif

# rtld.c and symbol.c use alloca, so disable SSP warnings.
COPTS.rtld.c+=	-Wno-stack-protector
COPTS.symbol.c+=-Wno-stack-protector

LDADD+=		-L${CLIBOBJ} -L${DESTDIR}${LIBDIR}
.if ${MKPICLIB} != "no"
LDADD+=		-lc_pic
.if ${MKPICINSTALL} != "no"
DPADD+=		${LIBC_PIC}
.endif
DPADD+=		${CLIBOBJ}/libc_pic.a
.else
LDADD+=		-lc
DPADD+=		${CLIBOBJ}/libc.a
.endif

.if ${MACHINE_CPU} == "hppa" || ${MACHINE_CPU} == "sh3"
# for $$divU, $$remU etc. (millicode)
LDADD+=		-lgcc
DPADD+=		${LIBGCC}
.endif

STRIPFLAG=

.PATH: $M


${PROG}: ${OBJS} ${DPADD}
	${_MKMSG_LINK} ${PROG}
	${CC} ${LDFLAGS} -o ${PROG} ${OBJS} ${LDADD}
.if ${MKDEBUG} != "no"
	(  ${OBJCOPY} --only-keep-debug ${.TARGET} ${.TARGET}.debug \
	   && ${OBJCOPY} --strip-debug -p -R .gnu_debuglink \
	   --add-gnu-debuglink=${.TARGET}.debug ${.TARGET} \
	) || (rm -f ${.TARGET}.debug; false)

_PROGDEBUG.${PROG} := ${PROG}.debug
.endif


.if ${SHLINKINSTALLDIR} != "/usr/libexec"
SYMLINKS+=	${SHLINKINSTALLDIR}/${PROG} /usr/libexec/${PROG}
.if ${MKDEBUG} != "no"
SYMLINKS+=	${DEBUGDIR}${BINDIR}/${PROG}.debug \
		${DEBUGDIR}/usr${BINDIR}/${PROG}.debug
.endif
.endif

.include <bsd.prog.mk>
.else

MAN=		ld.elf_so.1

.include <bsd.man.mk>
.endif
