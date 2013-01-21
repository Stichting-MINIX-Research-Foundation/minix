#	$NetBSD: Makefile,v 1.22 2012/03/21 05:37:44 matt Exp $

.include <bsd.own.mk>

.if defined(__MINIX)
CPPFLAGS+= 	-D_NETBSD_SOURCE
.endif
USE_SHLIBDIR=	yes

LIB=		terminfo
WARNS?=		5

CPPFLAGS+=	-I${.CURDIR}

SRCS=		term.c ti.c setupterm.c curterm.c tparm.c tputs.c
SRCS+=		compile.c hash.c
INCS=		term.h
INCSDIR=	/usr/include

COPTS.tparm.c = -Wno-format-nonliteral

MAN=		terminfo.3 terminfo.5
MLINKS=		terminfo.3 setupterm.3 \
		terminfo.3 set_curterm.3 terminfo.3 del_curterm.3 \
		terminfo.3 termname.3 terminfo.3 longname.3 \
		terminfo.3 tigetnum.3 terminfo.3 tigetflag.3 \
		terminfo.3 tigetstr.3 terminfo.3 tparm.3 terminfo.3 tputs.3 \
		terminfo.3 putp.3 \
		terminfo.3 ti_setupterm.3 terminfo.3 ti_getflag.3 \
		terminfo.3 ti_getnum.3 terminfo.3 ti_getstr.3 \
		terminfo.3 tiparm.3 terminfo.3 ti_tiparm.3 \
		terminfo.3 ti_puts.3 terminfo.3 ti_putp.3

# Build in termcap emulation
SRCS+=		termcap.c
INCS+=		termcap.h
MAN+=		termcap.3
MLINKS+=	termcap.3 tgetent.3 termcap.3 tgetflag.3 termcap.3 tgetnum.3 \
		termcap.3 tgetstr.3 termcap.3 tgoto.3

CPPFLAGS+=	-I${.OBJDIR}

.include <Makefile.hash>

# Generate our man pages
terminfo.5: genman terminfo.5.in term.h termcap_map.c
		@echo "Generating terminfo man pages"
		${SCRIPT_ENV} ${HOST_SH} ${.ALLSRC} > ${.TARGET}

CLEANFILES+=	terminfo.5

man: terminfo.5

.include <bsd.shlib.mk>

.if ${MKLINKLIB} != "no"
SYMLINKS+=	libterminfo.a ${LIBDIR}/libtermcap.a
SYMLINKS+=	libterminfo.a ${LIBDIR}/libtermlib.a
.endif

.if ${MKPROFILE} != "no"
SYMLINKS+=	libterminfo_p.a ${LIBDIR}/libtermcap_p.a
SYMLINKS+=	libterminfo_p.a ${LIBDIR}/libtermlib_p.a
.endif

.if ${MKPIC} != "no"

.if ${MKPICINSTALL} != "no"
SYMLINKS+=	libterminfo_pic.a ${LIBDIR}/libtermcap_pic.a
SYMLINKS+=	libterminfo_pic.a ${LIBDIR}/libtermlib_pic.a
.endif

.if exists(${.CURDIR}/shlib_version)
SYMLINKS+=	libterminfo.so.${SHLIB_FULLVERSION} \
		${_LIBSODIR}/libtermcap.so.0.6
SYMLINKS+=	libterminfo.so.${SHLIB_FULLVERSION} \
		${_LIBSODIR}/libtermlib.so.0.6
.if ${_LIBSODIR} != ${LIBDIR}
SYMLINKS+=	libterminfo.so.${SHLIB_FULLVERSION} \
		${LIBDIR}/libtermcap.so.0.6
SYMLINKS+=	libterminfo.so.${SHLIB_FULLVERSION} \
		${LIBDIR}/libtermlib.so.0.6
.endif

SYMLINKS+=	libterminfo.so.${SHLIB_MAJOR} \
		${_LIBSODIR}/libtermcap.so.0
SYMLINKS+=	libterminfo.so ${_LIBSODIR}/libtermcap.so
SYMLINKS+=	libterminfo.so.${SHLIB_MAJOR} \
		${_LIBSODIR}/libtermlib.so.0
SYMLINKS+=	libterminfo.so ${_LIBSODIR}/libtermlib.so
.if ${_LIBSODIR} != ${LIBDIR}
SYMLINKS+=	libterminfo.so.${SHLIB_MAJOR} \
		${LIBDIR}/libtermcap.so.0
SYMLINKS+=	libterminfo.so ${LIBDIR}/libtermcap.so
SYMLINKS+=	libterminfo.so.${SHLIB_MAJOR} \
		${LIBDIR}/libtermlib.so.0
SYMLINKS+=	libterminfo.so ${LIBDIR}/libtermlib.so
.endif
.endif	# exists shlib_version

.endif	# ${MKPIC} != "no"

.include <bsd.lib.mk>
