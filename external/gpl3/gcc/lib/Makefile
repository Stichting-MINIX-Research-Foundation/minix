#	$NetBSD: Makefile,v 1.4 2012/04/18 19:22:03 christos Exp $

# crtstuff is built out of elsewhere, or not at all
.if defined(__MINIX)
SUBDIR+= libgcc
.else
SUBDIR+= libgcc libiberty libobjc libsupc++ libstdc++-v3 libgomp
SUBDIR+= libmudflap libmudflapth
.endif

.include <bsd.subdir.mk>
