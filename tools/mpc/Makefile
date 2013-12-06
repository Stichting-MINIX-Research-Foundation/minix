#	$NetBSD: Makefile,v 1.3 2013/11/28 12:08:57 mrg Exp $
#

GNUHOSTDIST=	${.CURDIR}/../../external/lgpl3/mpc/dist

CONFIGURE_ARGS+=	--with-gmp=${TOOLDIR:Q} \
			--with-mpfr=${TOOLDIR:Q}

CONFIGURE_ARGS+=--disable-shared

.include "${.CURDIR}/../Makefile.gnuhost"

.if ${HOST_OSTYPE:C/\-.*//} != "Minix"
# Force avoiding possibly non-executable install-sh.
CONFIGURE_ENV+= ac_cv_path_mkdir="${TOOLDIR}/bin/${MACHINE_GNU_PLATFORM}-install -d"
.endif
