#	$NetBSD: Makefile,v 1.5 2011/07/19 15:59:02 tron Exp $

LIBISPRIVATE=	yes

# For ../Makefile.inc and bsd.own.mk
.include <bsd.init.mk>

LIB=		frontend

SRCS=		gcc.c version.c intl.c prefix.c opts-common.c gcc-options.c

# XXX
.if ${MACHINE_ARCH} == "x86_64" || ${MACHINE_ARCH} == "i386"
SRCS+=		driver-i386.c
.PATH:		${DIST}/gcc/config/i386
.endif
.if ${MACHINE_ARCH} == "powerpc" || ${MACHINE_ARCH} == "powerpc64"
SRCS+=		driver-rs6000.c
.PATH:		${DIST}/gcc/config/rs6000
.endif

CPPFLAGS+=	-I${GCCARCH} -I${BACKENDOBJ} -I. \
		${G_ALL_CFLAGS:M-D*} ${G_INCLUDES:M-I*:N-I.*} \
		-DPREFIX=\"/usr\" \
		-DDEFAULT_TARGET_VERSION=\"${G_version}\" \
		-DDEFAULT_TARGET_MACHINE=\"${MACHINE_GNU_PLATFORM}\" \
		-DCONFIGURE_SPECS="\"\""
HOST_CPPFLAGS+=	-I${GCCARCH} ${G_ALL_CFLAGS:M-D*} ${G_INCLUDES:M-I*:N-I.*}

DPSRCS+=	specs.h

MKPIC:=		no
MKPICLIB:=	no

.include <bsd.lib.mk>

CFLAGS+=	-Wno-stack-protector

.PATH: ${DIST}/gcc

# Build the specs.h file
CLEANFILES+=	specs.h
specs.h: Makefile
	for f in cp objc; do \
		echo "#include \"$$f/lang-specs.h\""; \
	done > specs.h

# Build the options files
optionlist: ${G_ALL_OPT_FILES} ${GCCARCH}/defs.mk Makefile ${DIST}/gcc/opt-gather.awk
	${TOOL_AWK} -f ${DIST}/gcc/opt-gather.awk ${G_ALL_OPT_FILES} > ${.TARGET}

CPPFLAGS.gcc-options.c=	-DGCC_DRIVER
gcc-options.c: optionlist
	${TOOL_AWK} -f ${DIST}/gcc/opt-functions.awk -f ${DIST}/gcc/optc-gen.awk \
		-v header_name="config.h system.h coretypes.h tm.h" < optionlist > ${.TARGET}
CLEANFILES+=	optionlist gcc-options.c
