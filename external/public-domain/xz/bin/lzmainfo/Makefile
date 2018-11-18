# $NetBSD: Makefile,v 1.2 2016/01/28 19:40:29 christos Exp $

PROG=	lzmainfo
BINDIR=	/usr/bin
SRCS=	lzmainfo.c

#args.c coder.c file_io.c hardware.c list.c main.c \
#	message.c options.c signals.c suffix.c util.c
SRCS+=	tuklib_exit.c tuklib_cpucores.c tuklib_open_stdxxx.c \
	tuklib_mbstr_fw.c tuklib_mbstr_width.c \
	tuklib_physmem.c tuklib_progname.c

#USETBL=	yes

.include <bsd.init.mk>

CPPFLAGS+=	-DPACKAGE=\"xz\"
.if !defined(__MINIX)
CPPFLAGS+=	-DENABLE_NLS -DLOCALEDIR=\"/usr/share/locale\"

DPADD+=	${LIBLZMA} ${LIBINTL} ${LIBPTHREAD}
LDADD+=	-llzma -lintl -lpthread
.else
DPADD+= ${LIBLZMA}
LDADD+= -llzma
.endif # !defined(__MINIX)

.PATH:	${XZSRCDIR}/src/common ${XZSRCDIR}/src/lzmainfo

.include <bsd.prog.mk>
