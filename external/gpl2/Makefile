#	$NetBSD: Makefile,v 1.5 2009/04/08 17:24:55 christos Exp $

.include <bsd.own.mk>

.if defined(__MINIX) && 0
# LSC FIXME: Hackish way to trigger the fetch phase from here
# 	     in the same way as from tools.
MODULE=	gmake

.include "${.CURDIR}/../../tools/Makefile.gnuhost"

# Force the mapping to standard targets even when not building tools
# We considere work to be done as soon as the fetch step is done,
# as everything else has to be triggered from /tools or ../usr.bin

.if ${USETOOLS} != "yes"
realall realinstall: ${GNUHOSTDIST:H}/.gitignore
.endif # defined(__MINIX)

.if (${MKCVS} != "no")
SUBDIR+= xcvs
.endif
.if (${MKLVM} != "no")
SUBDIR+= lvm2
.endif

.include <bsd.subdir.mk>
