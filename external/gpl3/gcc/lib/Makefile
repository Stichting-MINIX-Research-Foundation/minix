#	$NetBSD: Makefile,v 1.8 2014/10/22 21:21:02 christos Exp $

.include <bsd.own.mk>

# crtstuff is built out of elsewhere, or not at all
SUBDIR+= libgcc libiberty libobjc libgomp
#MINIX:
SUBDIR+= libbacktrace
SUBDIR+= liblto_plugin

.if (${MKLIBSTDCXX} != "no")
SUBDIR+= libsupc++ libstdc++-v3
.endif

.if !defined(__MINIX)
SUBDIR+= libasan
.endif # !defined(__MINIX)

.include <bsd.subdir.mk>
