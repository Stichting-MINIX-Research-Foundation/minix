#	$NetBSD: Makefile,v 1.8 2013/07/10 16:47:17 reinoud Exp $
#

.PATH:  ${.CURDIR}/../../../../fs/udf

LIB=	rumpfs_udf

SRCS=	udf_osta.c udf_vfsops.c udf_vnops.c udf_subr.c \
	udf_allocation.c udf_readwrite.c udf_strat_bootstrap.c \
	udf_strat_direct.c udf_strat_sequential.c udf_strat_rmw.c \
	udf_rename.c

CWARNFLAGS.clang+=	-Wno-self-assign

.include <bsd.lib.mk>
.include <bsd.klinks.mk>
