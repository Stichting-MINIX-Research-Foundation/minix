#	$NetBSD: Makefile,v 1.59 2012/06/04 19:06:45 joerg Exp $

HOSTLIB=	nbcompat

.ifndef __MINIX
SRCS=		atoll.c basename.c cdbr.c cdbw.c dirname.c \
		fgetln.c flock.c fparseln.c fpurge.c \
		getcap.c getline.c getmode.c getopt_long.c gettemp.c \
		heapsort.c \
		issetugid.c lchflags.c lchmod.c lchown.c libyywrap.c \
		md2.c md2hl.c md4c.c md4hl.c md5c.c md5hl.c \
		mi_vector_hash.c mkdtemp.c \
		mkstemp.c pread.c putc_unlocked.c pwcache.c pwrite.c \
		pw_scan.c \
		raise_default_signal.c rmd160.c rmd160hl.c \
		setenv.c setgroupent.c \
		setpassent.c setprogname.c sha1.c sha1hl.c sha2.c \
		sha256hl.c sha384hl.c sha512hl.c snprintf.c stat_flags.c \
		strlcat.c strlcpy.c strmode.c strndup.c strsep.c strsuftoll.c \
		strtoll.c unvis.c vis.c err.c errx.c verr.c verrx.c \
		vwarn.c vwarnx.c warn.c warnx.c fts.c glob.c efun.c
.else
SRCS=		atoll.c basename.c dirname.c fgetln.c flock.c fparseln.c \
		fpurge.c getline.c getmode.c getopt_long.c gettemp.c \
		heapsort.c \
		issetugid.c lchflags.c lchmod.c lchown.c libyywrap.c \
		md2.c md2hl.c md4c.c md4hl.c md5c.c md5hl.c \
		mi_vector_hash.c mkdtemp.c \
		mkstemp.c pread.c putc_unlocked.c pwcache.c pwrite.c \
		pw_scan.c \
		raise_default_signal.c rmd160.c rmd160hl.c \
		setenv.c setgroupent.c \
		setpassent.c setprogname.c sha1.c sha1hl.c sha2.c \
		sha256hl.c sha384hl.c sha512hl.c snprintf.c stat_flags.c \
		strlcat.c strlcpy.c strmode.c strndup.c strsep.c strsuftoll.c \
		strtoll.c unvis.c vis.c err.c errx.c verr.c verrx.c \
		vwarn.c vwarnx.c warn.c warnx.c fts.c glob.c efun.c
.endif

BUILD_OSTYPE!=  uname -s

# Disable use of pre-compiled headers on Darwin.
.if ${BUILD_OSTYPE} == "Darwin"
CPPFLAGS+=	-no-cpp-precomp
.endif

# -D_FILE_OFFSET_BITS=64 produces a much more amenable `struct stat', and
# other file ops, on many systems, without changing function names.

CPPFLAGS+=	-I. -I./include -I${.CURDIR} -I${.CURDIR}/sys \
		-DHAVE_NBTOOL_CONFIG_H=1 -D_FILE_OFFSET_BITS=64

.PATH:		${.CURDIR}/../../lib/libc/cdb \
		${.CURDIR}/../../lib/libc/gen \
		${.CURDIR}/../../lib/libc/hash \
		${.CURDIR}/../../lib/libc/hash/md2 \
		${.CURDIR}/../../lib/libc/hash/md5 \
		${.CURDIR}/../../lib/libc/hash/rmd160 \
		${.CURDIR}/../../lib/libc/hash/sha1 \
		${.CURDIR}/../../lib/libc/hash/sha2 \
		${.CURDIR}/../../lib/libc/md \
		${.CURDIR}/../../lib/libc/stdio \
		${.CURDIR}/../../lib/libc/stdlib \
		${.CURDIR}/../../lib/libc/string \
		${.CURDIR}/../../lib/libutil \
		${.CURDIR}/../../common/lib/libc/string \
		${.CURDIR}/../../common/lib/libc/hash/rmd160 \
		${.CURDIR}/../../common/lib/libc/hash/sha1 \
		${.CURDIR}/../../common/lib/libc/hash/sha2 \
		${.CURDIR}/../../common/lib/libc/md \
		${.CURDIR}/../../common/lib/libc/stdlib \
		${.CURDIR}/../../external/bsd/flex/dist

DPSRCS+=	defs.mk
CLEANFILES+=	config.log config.status configure.lineno *.stamp

# Get components of Berkeley DB.
_CURDIR:=	${.CURDIR}
.CURDIR:=	${_CURDIR}/../../lib/libc
.include "${.CURDIR}/db/Makefile.inc"
.CURDIR:=	${_CURDIR}

SRCS:=		${SRCS:M*.c}

config.cache: include/.stamp configure nbtool_config.h.in defs.mk.in
	rm -f ${.TARGET}
	CC=${HOST_CC:Q} CFLAGS=${HOST_CFLAGS:Q} LDFLAGS=${HOST_LDFLAGS:Q} \
		${HOST_SH} ${.CURDIR}/configure --cache-file=config.cache

defs.mk: config.cache
	@touch ${.TARGET}

# Run "${TOOLDIR}/bin/nbmake-${MACHINE} regen" by hand after editing
# configure.ac.  See more detailed instructions in configure.ac.
regen:
	cd ${.CURDIR} && ${TOOLDIR}/bin/${_TOOL_PREFIX}autoconf
	cd ${.CURDIR} && ${TOOLDIR}/bin/${_TOOL_PREFIX}autoheader

include/.stamp:
	mkdir -p include/sys include/machine include/rpc include/arpa
	@touch ${.TARGET}

cleandir:
	-rm -f nbtool_config.h confdefs.h defs.mk
	-rm -r -f include
	-rm -f config.cache

HOST_CPPFLAGS:=	${CPPFLAGS}
CPPFLAGS:=	# empty

.include <bsd.hostlib.mk>
