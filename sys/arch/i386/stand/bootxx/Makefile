# $NetBSD: Makefile,v 1.14 2010/12/29 17:44:03 jakllsch Exp $

SUBDIR=
#SUBDIR=	bootxx_ffsv1 .WAIT bootxx_ffsv2 bootxx_lfsv1 bootxx_lfsv2
#SUBDIR+=bootxx_msdos bootxx_ustarfs

# Ext2fs doesn't have enough free space (it has only 1KB)
# to store this primary bootloader, but we can put it into
# an independent small 'boot' partition as NetBSD/hp300 does.
#
# However MINIX implements a solution using the empty space before subpartition
.if !empty(CC:M*pcc)
# PCC cannot compile bootxx_ext2fs because it lacks regparm aka compactcall
SUBDIR+=bootxx_minixfs3
.else
SUBDIR+=bootxx_ext2fs bootxx_minixfs3
.endif

LIBOBJ= ${.OBJDIR}
.MAKEOVERRIDES+= LIBOBJ

.include <bsd.subdir.mk>
.include <bsd.obj.mk>

cleandir distclean: .WAIT cleanlibdir

cleanlibdir:
	-rm -rf lib
