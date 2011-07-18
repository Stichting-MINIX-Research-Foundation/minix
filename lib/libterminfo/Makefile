#	$NetBSD: Makefile,v 1.12 2010/07/06 05:59:53 mrg Exp $

.if defined(__MINIX)
.include "minix-config.inc"
USE_SHLIBDIR=	no
.else
USE_SHLIBDIR=	yes
.endif

LIB=		terminfo
WARNS=		4

CPPFLAGS+=	-I${.CURDIR}

SRCS=		term.c ti.c setupterm.c curterm.c tparm.c tputs.c
SRCS+=		compile.c hash.c
INCS=		term.h
INCSDIR=	/usr/include

MAN=		terminfo.3 terminfo.5
MLINKS=		terminfo.3 setupterm.3 \
		terminfo.3 set_curterm.3 terminfo.3 del_curterm.3 \
		terminfo.3 tigetnum.3 terminfo.3 tigetflag.3 \
		terminfo.3 tigetstr.3 terminfo.3 tparm.3 terminfo.3 tputs.3 \
		terminfo.3 putp.3 \
		terminfo.3 ti_setupterm.3 terminfo.3 ti_getflag.3 \
		terminfo.3 ti_getnum.3 terminfo.3 ti_getstr.3 \
		terminfo.3 t_parm.3 terminfo.3 t_vparm.3 \
		terminfo.3 ti_puts.3 terminfo.3 ti_putp.3

# Build in termcap emulation
SRCS+=		termcap.c
INCS+=		termcap.h
MAN+=		termcap.3
MLINKS+=	termcap.3 tgetent.3 termcap.3 tgetflag.3 termcap.3 tgetnum.3 \
		termcap.3 tgetstr.3 termcap.3 tgoto.3

# Generate our string and hash tables
hash:
		@echo "Generating terminfo hash"
		cd ${.CURDIR} && ${HOST_SH} ./genhash >hash.c
		@echo "Generating termcap hash"
		cd ${.CURDIR} && ${HOST_SH} ./genthash >termcap_hash.c

# Generate our man pages
terminfo.5: terminfo.5.in term.h termcap_map.c
		@echo "Generating terminfo man pages"
		cd ${.CURDIR} && ${HOST_SH} ./genman >${.OBJDIR}/$@

CLEANFILES+=	terminfo.5

# Allow terminfo descriptions to be compiled into libterminfo
compiled_terms:
		@echo "Generating compiled terminfo descriptions"
		cd ${.CURDIR} && ${HOST_SH} ./genterms >compiled_terms.c

man: terminfo.5

gen: hash compiled_terms man

.include <bsd.own.mk>
.if !defined(__MINIX)
.include <bsd.shlib.mk>
.endif

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
