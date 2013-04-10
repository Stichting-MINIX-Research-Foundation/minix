#	$NetBSD: Makefile,v 1.136 2012/08/10 12:11:30 joerg Exp $

.include <bsd.own.mk>

#SUBDIR+=	bc
#SUBDIR+=	c89 c99
#SUBDIR+=	dc diffutils gkermit
SUBDIR+=	texinfo

.if !defined(__MINIX)
.if ${MKBSDGREP} == "no"
SUBDIR+=	grep
.endif

.if ${MKGROFF} != "no"
SUBDIR+=	groff
.endif

SUBDIR+=	gettext

.if ${MKGDB} != "no"
.if ${HAVE_GDB} == "6"
SUBDIR+=	gdb6
.endif
.endif

.if ${MKGCC} != "no"
.if ${HAVE_GCC} == 4
.if ${MKGCCCMDS} != "no"
SUBDIR+=	gcc4
.endif
.endif
.endif
.endif #!defined(__MINIX)

.include <bsd.subdir.mk>
