#	$NetBSD: Makefile,v 1.10 2019/09/23 13:42:33 christos Exp $
#

# Install CD, to be made after 'build.sh release'
# Requires populated $RELEASEDIR/${MACHINE}

.include <bsd.own.mk>
.include "${.CURDIR}/../Makefile.cdrom"

CDBASE=		i386cd			# gives ${CDBASE}.iso
CDRELEASE=	true                    # include $RELEASEDIR/$RELEASEMACHINEDIR
CDRELEASE_NODEBUG=	true
CDBUILDEXTRA+=	boot.cfg		# Add boot.cfg file
CLEANFILES+=	boot.cfg

prepare_md_post:
	${TOOL_SED} "s/@@VERSION@@/${DISTRIBVER}/" < ${.CURDIR}/boot.cfg.in > boot.cfg

.include "${.CURDIR}/../../../common/Makefile.bootcd"
