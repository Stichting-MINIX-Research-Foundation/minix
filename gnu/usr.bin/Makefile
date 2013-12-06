#	$NetBSD: Makefile,v 1.137 2012/11/06 12:25:56 skrll Exp $

.include <bsd.own.mk>

.if defined(__MINIX)
SUBDIR+=	texinfo
.else
SUBDIR+=	bc
SUBDIR+=	c89 c99
SUBDIR+=	dc diffutils gkermit
SUBDIR+=	rcs send-pr texinfo

.if ${MKBSDGREP} == "no"
SUBDIR+=	grep
.endif

.if ${MKGROFF} != "no"
SUBDIR+=	groff
.endif

SUBDIR+=	gettext

.if ${MKGCC} != "no"
.if ${HAVE_GCC} == 4
.if ${MKGCCCMDS} != "no"
SUBDIR+=	gcc4
.endif
.endif
.endif
.endif # defined(__MINIX)

.include <bsd.subdir.mk>
