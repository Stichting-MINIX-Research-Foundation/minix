#	$NetBSD: Makefile,v 1.13 2017/05/21 15:28:37 riastradh Exp $

.include <bsd.own.mk>

SUBDIR=
#SUBDIR+=	ramdisks
SUBDIR+=	.WAIT
#SUBDIR+=	instkernel
.if ${MKKMOD} != "no"
SUBDIR+=	kmod
SUBDIR+=	kmod-cgdroot
.endif
SUBDIR+=	.WAIT
#SUBDIR+=	cdroms
#SUBDIR+=	floppies
SUBDIR+=	liveimage
SUBDIR+=	installimage
TARGETS+=	release

# Build an x86_64 UEFI El Torito ISO.  The legacy cdroms target uses the
# 32-bit BIOS boot path and is not suitable for the amd64 EFI loader.
iso_image: .PHONY
	ARCH=amd64 \
	OBJ=${.OBJDIR:Q}/../.. \
	DESTDIR=${DESTDIR:Q} \
	RELEASEDIR=${RELEASEDIR:Q} \
	SETS_DIR=${RELEASEDIR:Q}/${RELEASEMACHINEDIR}/binary/sets \
	CROSS_TOOLS=${TOOLDIR:Q}/bin \
	CREATE_IMAGE_ONLY=1 \
	bash ${NETBSDSRCDIR:Q}/releasetools/x86_64_uefi_cdimage.sh

live_image:
	${MAKEDIRTARGET} liveimage live_image

install_image:
	${MAKEDIRTARGET} installimage install_image

.include <bsd.subdir.mk>
