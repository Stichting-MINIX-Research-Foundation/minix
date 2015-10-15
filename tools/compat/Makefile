#	$NetBSD: Makefile,v 1.76 2015/09/17 14:25:43 christos Exp $

HOSTLIB=	nbcompat

.include <bsd.own.mk>

SRCS=		atoll.c basename.c cdbr.c cdbw.c dirname.c \
		dprintf.c fgetln.c flock.c fparseln.c fpurge.c \
		getcap.c getline.c getmode.c getopt_long.c gettemp.c \
		heapsort.c \
		issetugid.c lchflags.c lchmod.c lchown.c libyywrap.c \
		md2.c md2hl.c md4c.c md4hl.c md5c.c md5hl.c \
		mi_vector_hash.c mkdtemp.c \
		mkstemp.c pread.c putc_unlocked.c pwcache.c pwrite.c \
		pw_scan.c \
		raise_default_signal.c reallocarr.c rmd160.c rmd160hl.c \
		setenv.c setgroupent.c \
		setpassent.c setprogname.c sha1.c sha1hl.c sha2.c \
		sha256hl.c sha384hl.c sha512hl.c snprintb.c snprintf.c \
		stat_flags.c strlcat.c strlcpy.c strmode.c \
		strndup.c strnlen.c \
		strsep.c strsuftoll.c strtoll.c \
		strtoi.c strtou.c \
		unvis.c vis.c err.c errx.c errc.c verr.c verrx.c verrc.c \
		vwarn.c vwarnx.c vwarnc.c warn.c warnx.c warnc.c \
		fts.c glob.c efun.c

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
		${.CURDIR}/../../common/lib/libc/cdb \
		${.CURDIR}/../../common/lib/libc/string \
		${.CURDIR}/../../common/lib/libc/hash/rmd160 \
		${.CURDIR}/../../common/lib/libc/hash/sha1 \
		${.CURDIR}/../../common/lib/libc/hash/sha2 \
		${.CURDIR}/../../common/lib/libc/md \
		${.CURDIR}/../../common/lib/libc/stdlib \
		${.CURDIR}/../../common/lib/libutil \
		${.CURDIR}/../../external/bsd/flex/dist

DPSRCS+=	defs.mk
CLEANFILES+=	config.log config.status configure.lineno *.stamp
CLEANDIRFILES+=	defs.mk config.cache confdefs.h

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

INCFILES=	nbtool_config.h
INCSUBDIRS=	sys machine rpc arpa
CLEANDIRFILES+= ${INCFILES}

# CLEANDIRFILES may not contain directory names
cleandir:	cleandir.include
cleandir.include: .PHONY
		rm -rf include

include/.stamp:
	mkdir -p ${INCSUBDIRS:@d@ include/$d @}
	@touch ${.TARGET}

# Install rules

HOST_LIBDIR=	${TOOLDIR}/lib
HOST_INCSDIR=	${TOOLDIR}/include
HOST_SHAREDIR= ${TOOLDIR}/share

install:	.PHONY install.lib includes install.defs.mk

# Install lib${HOSTLIB}.a in ${TOOLDIR}/lib
install.lib: .PHONY ${HOST_LIBDIR}/lib${HOSTLIB}.a
${HOST_LIBDIR}/lib${HOSTLIB}.a: lib${HOSTLIB}.a
	${_MKTARGET_INSTALL}
	${HOST_INSTALL_DIR} ${HOST_LIBDIR}
	${HOST_INSTALL_FILE} -m ${LIBMODE} ${.ALLSRC} ${.TARGET}

.for _f in ${INCFILES}
HOST_INCINSTFILES+= ${HOST_INCSDIR}/compat/${_f}
${HOST_INCSDIR}/compat/${_f}: ${_f}
	${_MKTARGET_INSTALL}
	${HOST_INSTALL_FILE} ${.ALLSRC} ${.TARGET}
.endfor

.for _d in ${INCSUBDIRS}
HOST_INCINSTDIRS+= ${HOST_INCSDIR}/compat/${_d}
${HOST_INCSDIR}/compat/${_d}:
	${_MKTARGET_INSTALL}
	${HOST_INSTALL_DIR} ${.TARGET}
.endfor

# Install include files in ${TOOLDIR}/include/compat
includes: .PHONY ${HOST_INCINSTDIRS} .WAIT ${HOST_INCINSTFILES}
	@(cd include && find . -name '*.h' -print | while read f ; do \
	    ${HOST_INSTALL_FILE} $$f ${HOST_INCSDIR}/compat/$$f ; \
	done)


# Install defs.mk in ${TOOLDIR}/share/compat
install.defs.mk: .PHONY ${HOST_SHAREDIR}/compat/defs.mk
${HOST_SHAREDIR}/compat/defs.mk: defs.mk
	${_MKTARGET_INSTALL}
	${HOST_INSTALL_DIR} ${HOST_SHAREDIR}
	${HOST_INSTALL_DIR} ${HOST_SHAREDIR}/compat
	${HOST_INSTALL_FILE} ${.ALLSRC} ${.TARGET}

# bsd.hostlib.mk wants HOST_CPPFLAGS, not CPPFLAGS

HOST_CPPFLAGS:=	${CPPFLAGS}
CPPFLAGS:=	# empty

.include <bsd.hostlib.mk>

# Use uninstalled copy of host-mkdep
HOST_MKDEP_OBJ!= cd ${.CURDIR}/../host-mkdep && ${PRINTOBJDIR}
HOST_MKDEP=	${HOST_MKDEP_OBJ}/host-mkdep
MKDEP=		${HOST_MKDEP}

# Use uninstalled copy of the install program
INSTALL_OBJ!=	cd ${NETBSDSRCDIR}/tools/binstall && ${PRINTOBJDIR}
INSTALL=	${INSTALL_OBJ}/xinstall

# Run "${TOOLDIR}/bin/nbmake-${MACHINE} regen" by hand after editing
# configure.ac.  See more detailed instructions in configure.ac.
regen:
	cd ${.CURDIR} && ${TOOLDIR}/bin/${_TOOL_PREFIX}autoconf
	cd ${.CURDIR} && ${TOOLDIR}/bin/${_TOOL_PREFIX}autoheader
