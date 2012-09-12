#	$NetBSD: Makefile,v 1.7 2012/04/15 08:37:33 mrg Exp $
#

GNUHOSTDIST=	${.CURDIR}/../../external/bsd/file/dist

CONFIGURE_ENV+=	CC=${HOST_CC:Q} \
		CFLAGS=${HOST_CFLAGS:Q} \
		CPPFLAGS=${HOST_CPPFLAGS:Q} \
		CXX=${HOST_CXX:Q}

CONFIGURE_ARGS=	--program-prefix=${_TOOL_PREFIX} --disable-shared \
		--disable-warnings

.NOTPARALLEL:

.include "${.CURDIR}/../Makefile.gnuhost"

.if ${HOST_OSTYPE:C/\-.*//} != "Minix"
# Force avoiding possibly non-executable install-sh.
CONFIGURE_ENV+= ac_cv_path_mkdir="${TOOLDIR}/bin/${MACHINE_GNU_PLATFORM}-install -d"
.endif
