#	$NetBSD: Makefile,v 1.7 2013/12/25 23:35:17 christos Exp $
#

SMBROOT=${.CURDIR}/../../../external/bsd/smbfs

MOUNTNAME=	smbfs
SRCS+=		smb_rumpops.c

PROGDPLIBS+=	smb ${SMBROOT}/lib/libsmb
LDADD+= 	-lrumpnet_sockin -lrumpnet
LDADD+=		-lrumpkern_crypto -lrumpdev_netsmb -lrumpdev

ISRUMP=		# it is

.include "${SMBROOT}/Makefile.inc"

CPPFLAGS+=-I${SMBDIST}/mount_smbfs
.PATH: ${SMBDIST}/mount_smbfs

.include <bsd.prog.mk>
