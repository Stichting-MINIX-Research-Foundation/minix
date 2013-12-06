#	$NetBSD: Makefile,v 1.6 2013/08/22 16:03:31 matt Exp $

.include <bsd.own.mk>
.include <bsd.init.mk>

.include "${.CURDIR}/../../arch/${GCC_MACHINE_ARCH}/defs.mk"

.cc: # disable .cc->NULL transform

DIST=		${NETBSDSRCDIR}/external/gpl3/gcc/dist
CONF=		${DIST}/libstdc++-v3/config

INCS=		${G_bits_headers} ${G_c_base_headers_extra} ${G_thread_host_headers} ${G_host_headers_extra} ${G_host_headers}
INCS+=		c++config.h
INCSDIR=	/usr/include/g++/bits

CLEANFILES+=	basic_file.h c++io.h c++locale.h

basic_file.h: ${CONF}/io/basic_file_stdio.h
	cp $> ${.TARGET}
c++io.h: ${CONF}/io/c_io_stdio.h
	cp $> ${.TARGET}
c++locale.h: ${CONF}/locale/generic/c_locale.h
	cp $> ${.TARGET}

# List of places to find cpu files; maybe do this differently from
# mknative?
BITS_CPUDIR.sparc64=sparc

_DIR=${BITS_CPUDIR.${GCC_MACHINE_ARCH}}
.if ${_DIR} != ""
BITS_CPUDIR=${BITS_CPUDIR.${GCC_MACHINE_ARCH}}
.else
BITS_CPUDIR=${MACHINE_GNU_ARCH}
.endif

.PATH: ${.CURDIR}/../../arch/${GCC_MACHINE_ARCH} \
	${DIST}/libstdc++-v3/include/bits \
	${DIST}/libstdc++-v3/include/c_std \
	${DIST}/libstdc++-v3/include/precompiled \
	${CONF}/os/bsd/netbsd \
	${CONF}/os/generic \
	${CONF}/io \
	${CONF}/cpu/${BITS_CPUDIR} \
	${CONF}/cpu/generic \
	${CONF}/locale/generic

# Get default targets including <bsd.inc.mk>.
.include <bsd.prog.mk>
