#	$NetBSD: Makefile,v 1.3 2014/09/19 17:23:24 matt Exp $

# install multilib c++config.h files

.if ${MACHINE_ARCH} == "sparc64"
SUBDIR=		sparc sparc64
CXXCONFIGARGS=	sparc64 _LP64 sparc
.endif

.if ${MACHINE_ARCH} == "x86_64"
SUBDIR=		i386 x86_64
CXXCONFIGARGS=	x86_64 _LP64 i386
.endif

.if ${MACHINE_ARCH} == "mips64eb" || ${MACHINE_ARCH} == "mips64el"
SUBDIR=		mips64 mips32 mipso32
CXXCONFIGARGS=	mips64 _ABI64 mipso32 _ABIO32 mips32
.endif

.if ${MACHINE_ARCH} == "powerpc64"
SUBDIR=		powerpc powerpc64
CXXCONFIGARGS=	powerpc64 _LP64 powerpc
.endif

.if ${MACHINE_ARCH} == "riscv64"
SUBDIR=		riscv32 riscv64
CXXCONFIGARGS=	riscv64 _LP64 riscv32
.endif

# XXX arm variants are missing, but hopefully won't need them
# until aarch64 anyway.


# now install the generated front end

c++config.h: Makefile mkcxxconfig_h.sh
	${HOST_SH} ${.CURDIR}/mkcxxconfig_h.sh ${CXXCONFIGARGS} > ${.TARGET}.tmp && \
	    mv ${.TARGET}.tmp ${.TARGET}
CLEANFILES+=	c++config.h

INCS+=			c++config.h
#INCSNAME_c++config.h=	cxxconfig.h
INCSDIR=		/usr/include/g++/bits

.include <bsd.subdir.mk>
.include <bsd.inc.mk>
.include <bsd.files.mk>
