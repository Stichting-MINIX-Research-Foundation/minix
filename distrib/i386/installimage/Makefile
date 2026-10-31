#	$NetBSD: Makefile,v 1.19 2020/05/06 09:18:10 bouyer Exp $

.include <bsd.own.mk>

INSTIMGBASE=	Minix-${DISTRIBVER}-i386-install	# gives ${IMGBASE}.img

INSTIMAGEMB?=	1900			# for all installation binaries

#PRIMARY_BOOT=		bootxx_ffsv1
PRIMARY_BOOT=		bootxx_minixfs3
SECONDARY_BOOT=		boot_monitor
#SECONDARY_BOOT_ARG=	# unnecessary
EFIBOOT=		${WORKDIR}/usr/mdec/bootia32.efi

USE_MBR=		yes
USE_GPT=        no
USE_GPTMBR=		no

CLEANFILES+=	boot.cfg

prepare_md_post:
	${TOOL_SED}							\
	    -e "s/@@MACHINE@@/${MACHINE}/"				\
	    -e "s/@@VERSION@@/${DISTRIBVER}/"				\
	    < ${.CURDIR}/boot.cfg.in > boot.cfg

DISTRIBDIR!= cd ${.CURDIR}/../.. ; pwd
#SYSINSTDIR!= cd ${.CURDIR}/../../../usr.sbin/sysinst/arch/${MACHINE} && ${PRINTOBJDIR}

SPEC_EXTRA=		${.CURDIR}/spec.inst
IMGFILE_EXTRA=								\
	${.CURDIR}/etc.ttys		etc/ttys			\
	${.CURDIR}/etc.rc		etc/rc				\
	${.CURDIR}/install.sh		.				\
	${.OBJDIR}/boot.cfg		.				\
#	${SYSINSTDIR}/sysinstmsgs.de	.				\
#	${SYSINSTDIR}/sysinstmsgs.es	.				\
#	${SYSINSTDIR}/sysinstmsgs.fr	.				\
#	${SYSINSTDIR}/sysinstmsgs.pl	.				\
#	${SYSINSTDIR}/sysinst		.

.include "${DISTRIBDIR}/common/bootimage/Makefile.installimage"
