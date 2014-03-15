#	$NetBSD: Makefile,v 1.39 2010/04/23 19:41:02 joerg Exp $
#       @(#)Makefile	8.1 (Berkeley) 5/31/93

.include <bsd.own.mk>

PROG=   pax
SRCS=	ar_io.c ar_subs.c buf_subs.c file_subs.c ftree.c\
	gen_subs.c getoldopt.c options.c pat_rep.c pax.c sel_subs.c tables.c\
	tar.c tty_subs.c

.if defined(SMALLPROG)
CPPFLAGS+=	-DSMALL -DNO_CPIO
.else
SRCS+=		getid.c spec.c misc.c pack_dev.c cpio.c
CPPFLAGS+=	-I${NETBSDSRCDIR}/usr.sbin/mtree \
		-I${NETBSDSRCDIR}/sbin/mknod
.PATH:		${NETBSDSRCDIR}/usr.sbin/mtree \
		${NETBSDSRCDIR}/sbin/mknod

.if (${HOSTPROG:U} == "")
DPADD+= ${LIBUTIL}
LDADD+= -lutil
.endif
.endif

MAN=	pax.1 tar.1 cpio.1

.if defined(HOSTPROG)
CPPFLAGS+=	-DHOSTPROG
.else	# {	! HOSTPROG

# XXX: Interix does not have it; we need a conditional for it.
CPPFLAGS+=	-DHAVE_SYS_MTIO_H

.if ${MKBSDTAR} == "no"
LINKS+=	${BINDIR}/pax ${BINDIR}/tar
SYMLINKS+=${BINDIR}/tar /usr/bin/tar
.if defined(__MINIX)
SYMLINKS+=${BINDIR}/tar /usr/bin/bsdtar
.endif

LINKS+=	${BINDIR}/pax ${BINDIR}/cpio
SYMLINKS+=${BINDIR}/cpio /usr/bin/cpio
.endif
.endif	# }	! HOSTPROG

.if !defined(HOSTPROG) && !defined(SMALLPROG)
CPPFLAGS+=	-DSUPPORT_RMT

LDADD+=	-lrmt
DPADD+=	${LIBRMT}
.endif

.if defined(__MINIX)
CPPFLAGS+= -DHOSTPROG

MLINKS+= pax.1 bsdtar.1
.endif # defined(__MINIX)

.include <bsd.prog.mk>
