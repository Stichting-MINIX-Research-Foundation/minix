#	$NetBSD: Makefile,v 1.6 2013/12/01 20:15:42 christos Exp $
#

USE_FORT?= yes	# data driven bugs?

.include <bsd.own.mk>

.include "../Makefile.inc"

LIB=		magic
LIBDPLIBS+=	z	${NETBSDSRCDIR}/lib/libz
CPPFLAGS+=-I.

.if ${MACHINE_ARCH} == "i386"
# Set lint to exit on warnings
# XXX: Soon
#LINTFLAGS+=	-w
.endif

SRCS=		magic.c apprentice.c softmagic.c ascmagic.c compress.c	\
		is_tar.c readelf.c print.c fsmagic.c apptype.c funcs.c \
		cdf.c readcdf.c cdf_time.c encoding.c
MAN=		libmagic.3
MLINKS+=	libmagic.3 magic_open.3 \
		libmagic.3 magic_close.3 \
		libmagic.3 magic_error.3 \
		libmagic.3 magic_file.3 \
		libmagic.3 magic_buffer.3 \
		libmagic.3 magic_setflags.3 \
		libmagic.3 magic_check.3 \
		libmagic.3 magic_compile.3 \
		libmagic.3 magic_load.3
INCS=		magic.h
INCSDIR=	/usr/include

COPTS.softmagic.c = -Wno-format-nonliteral

magic.c:	magic.h
magic.h:        magic.h.in
	${TOOL_SED} -e "s/X.YY/${VERSION:S/.//g}/" < ${.ALLSRC} > ${.TARGET}
CLEANFILES+=	magic.h

.include <bsd.lib.mk>
