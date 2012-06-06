#	$NetBSD: Makefile,v 1.7 2012/02/28 20:26:07 joerg Exp $

REQUIRETOOLS=	yes
NOLINT=		# defined
UNSUPPORTED_COMPILER.clang=	# defined

.include <bsd.own.mk>

# If using an external toolchain, we expect crtbegin/crtend to be
# supplied by that toolchain's run-time support.
.if !defined(EXTERNAL_TOOLCHAIN) && ${MKGCC} != "no"

DIST=		${NETBSDSRCDIR}/external/gpl3/gcc/dist
GNUHOSTDIST=	${DIST}
GCCARCH=	${NETBSDSRCDIR}/external/gpl3/gcc/usr.bin/gcc/arch/${MACHINE_ARCH}

GALLCFLAGS=	${G_CRTSTUFF_CFLAGS} ${G_CRTSTUFF_T_CFLAGS}

CPPFLAGS+=	-I${GCCARCH} ${GALLCFLAGS:M-D*} ${GALLCFLAGS:M-I*:N-I.*}
CPPFLAGS+=	-I.
COPTS+=		-finhibit-size-directive \
		-fno-inline \
		-fno-exceptions \
		-fno-zero-initialized-in-bss \
		-fno-toplevel-reorder \
		-fno-tree-vectorize \
		-fno-omit-frame-pointer \
		-fno-asynchronous-unwind-tables

GCFLAGS=	${GALLCFLAGS:N-D*:N-I*:N-i*:N./*}

DPSRCS+=	${.CURDIR}/arch/${MACHINE_ARCH}.mk tconfig.h

.include "${.CURDIR}/arch/${MACHINE_ARCH}.mk"

SRCS+=		crtbegin.c crtend.c
OBJS+=		crtbegin.o crtend.o
.if ${MKPIC} != "no"
SRCS+=		crtbeginS.c crtendS.c
OBJS+=		crtbeginS.o crtendS.o	# for shared libraries
CPPFLAGS.crtbeginS.o+=	-fPIC -DPIC
CPPFLAGS.crtendS.o+=	-fPIC -DPIC
SRCS+=		crtbeginT.c
OBJS+=		crtbeginT.o		# for -static links
.endif

realall: ${OBJS}

FILES=${OBJS}
FILESDIR=${LIBDIR}

.PATH: ${DIST}/gcc ${DIST}/gcc/config ${G_CONFIGDIR}

.include "../Makefile.tconfigh"
EXTRA_FAKEHEADERS= options.h
.include "../Makefile.hacks"

${OBJS}: ${DPSRCS}

.include <bsd.prog.mk>

# Override the default .c -> .o rule.
.c.o:
	${_MKTARGET_COMPILE}
	${CC} ${CPPFLAGS} ${CPPFLAGS.${.TARGET}} ${GCFLAGS} ${COPTS} ${COPTS.${.TARGET}} ${CPUFLAGS} -c ${.IMPSRC} -o ${.TARGET}.o
	mv ${.TARGET}.o ${.TARGET}

.if (${MACHINE_ARCH} == "mips64eb") || (${MACHINE_ARCH} == "mips64el")
# Turn off as(1) warnings on MIPS, since warnings are fatal with WARNS>0
# and GCC configury passes -finhibit-size-directive which causes mips-gas
# to barf.  Don't know what the real fix for this is...
#
# XXX should be COPTS, but that's too early
CPUFLAGS+=-Wa,--no-warn
.endif

.else

.include <bsd.prog.mk>			# do nothing

.endif	# ! EXTERNAL_TOOLCHAIN && MKGCC != no
