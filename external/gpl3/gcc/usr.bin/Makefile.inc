#	$NetBSD: Makefile.inc,v 1.25 2014/07/27 04:16:09 dholland Exp $

.ifndef _EXTERNAL_GPL3_GCC_USR_BIN_MAKEFILE_INC_
_EXTERNAL_GPL3_GCC_USR_BIN_MAKEFILE_INC_=1

.include <bsd.own.mk>

.sinclude "../../Makefile.gcc_path"

GCC_MACHINE_ARCH=${MACHINE_ARCH:S/earmv5/earm/}
GMP_MACHINE_ARCH=${MACHINE_ARCH:C/armv[4-7]/arm/}

TOP=		${NETBSDSRCDIR}
DIST=		${GCCDIST}

GCCARCH=	${.CURDIR}/../gcc/arch/${GCC_MACHINE_ARCH}

CPPFLAGS+=	-DLOCALEDIR=\"/usr/share/locale\" -DNETBSD_NATIVE
HOST_CPPFLAGS+=	-I${.CURDIR}/..

# Link in the GMP/MPFR/MPC headers since we don't install them
BUILDSYMLINKS+=	${NETBSDSRCDIR}/external/lgpl3/gmp/lib/libgmp/arch/${GMP_MACHINE_ARCH}/gmp.h gmp.h
BUILDSYMLINKS+=	${NETBSDSRCDIR}/external/lgpl3/mpfr/dist/src/mpfr.h mpfr.h
BUILDSYMLINKS+=	${NETBSDSRCDIR}/external/lgpl3/mpfr/dist/src/mpf2mpfr.h mpf2mpfr.h
BUILDSYMLINKS+=	${NETBSDSRCDIR}/external/lgpl3/mpc/dist/src/mpc.h mpc.h
BUILDSYMLINKS+=	${NETBSDSRCDIR}/external/lgpl3/mpc/dist/src/mpc-log.h mpc-log.h
CPPFLAGS+=	-I.
# XXX
DPSRCS+= gmp.h mpfr.h mpf2mpfr.h mpc.h mpc-log.h

BACKENDOBJ!=		cd ${.CURDIR}/../backend && ${PRINTOBJDIR}
FRONTENDOBJ!=		cd ${.CURDIR}/../frontend && ${PRINTOBJDIR}

.if ${USETOOLS} == "yes"
NBCOMPATLIB=   -L${TOOLDIR}/lib -lnbcompat
.endif

GNUHOSTDIST=	${DIST}
BUILD_PREFIX=	${C_BUILD_PREFIX}

HOSTLIBIBERTYOBJ!=	cd ${.CURDIR}/../host-libiberty && ${PRINTOBJDIR}
HOSTLIBIBERTY=		${HOSTLIBIBERTYOBJ}/libiberty/libiberty.a

BASEVER!=		cat ${GNUHOSTDIST}/gcc/BASE-VER
.if exists(${GNUHOSTDIST}/gcc/DEV-PHASE)
DEVPHASE!=		cat ${GNUHOSTDIST}/gcc/DEV-PHASE
.else
DEVPHASE=
.endif
.if ${DEVPHASE} == "release" || ${DEVPHASE} == ""
DATESTAMP=
.else
DATESTAMP!=		cat ${GNUHOSTDIST}/gcc/DATESTAMP
.endif

BASEVERSTR=		"\"$(BASEVER)\""
.if !empty(DEVPHASE)
DEVPHASESTR=		"\" $(DEVPHASE)\""
.else
DEVPHASESTR=		"\"\""
.endif
.if !empty(DATESTAMP)
DATESTAMPSTR=		"\" $(DATESTAMP)\""
.else
DATESTAMPSTR=		"\"\""
.endif

.include "${TOP}/tools/gcc/gcc-version.mk"

# XXX pull this out of our configs
G_BUGURL=<http://www.NetBSD.org/support/send-pr.html>
G_BUGURL_s="\"${G_BUGURL}\""
G_PKGVERSION=(${NETBSD_GCC_VERSION}) 
G_PKGVERSION_s="\"${G_PKGVERSION} \""

VER_CPPFLAGS=	-DBASEVER="\"${BASEVER}"\" \
		-DDATESTAMP=$(DATESTAMP_s) \
		-DREVISION=$(REVISION_s) \
		-DDEVPHASE=$(DEVPHASE_s) \
		-DPKGVERSION=$(G_PKGVERSION_s) \
		-DBUGURL=$(G_BUGURL_s)

CPPFLAGS.version.c+=	${VER_CPPFLAGS}
version.c: ${GCCARCH}/defs.mk ${TOP}/tools/gcc/gcc-version.mk

.include "${GCCARCH}/defs.mk"

.if ${MKPIC} != "no"
.if ${G_ENABLE_SHARED} == "yes" && ${G_SHLIB_LINK} != ""
CPPFLAGS+=	-DENABLE_SHARED_LIBGCC
.endif

.if empty(G_SHLIB_MULTILIB)
CPPFLAGS+=	-DNO_SHARED_LIBGCC_MULTILIB
.endif
.endif

NOCLANGERROR=	# defined

.endif
