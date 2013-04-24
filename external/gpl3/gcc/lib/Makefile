#	$NetBSD: Makefile,v 1.4 2012/04/18 19:22:03 christos Exp $

# crtstuff is built out of elsewhere, or not at all
SUBDIR+= libgcc libiberty libobjc libsupc++ libstdc++-v3 libgomp

.if !defined(__MINIX)
SUBDIR+= libmudflap libmudflapth
.endif # !defined(__MINIX)

.include <bsd.subdir.mk>
