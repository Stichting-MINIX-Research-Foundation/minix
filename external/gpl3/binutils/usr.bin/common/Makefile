#	$NetBSD: Makefile,v 1.5 2009/11/10 10:24:50 skrll Exp $

.include <bsd.own.mk>

TOP=		${NETBSDSRCDIR}/external/gpl3/binutils

.include "${.CURDIR}/arch/${MACHINE_ARCH}/defs.mk"
.include "${.CURDIR}/../common/Makefile.inc"

TEXINFO=	${G_TEXINFOS}
INFOFLAGS=	-I${DIST}/binutils/doc -I${DIST}/libiberty -I{.CURDIR}

.PATH: ${DIST} ${DIST}/binutils/doc

${TEXINFO}: bfdver.texi

bfdver.texi: ${.CURDIR}/arch/${MACHINE_ARCH}/defs.mk
	${_MKTARGET_CREATE}
	rm -f ${.TARGET}
	(echo "@set VERSION ${G_VERSION}"; \
	if [ -n "$(G_PKGVERSION)" ]; then \
		echo "@set VERSION_PACKAGE $(G_PKGVERSION)"; \
	fi; \
	if [ -n "$(G_REPORT_BUGS_TEXI)" ]; then \
		echo "@set BUGURL $(G_REPORT_BUGS_TEXI)"; \
	fi \
	) > ${.TARGET}

CLEANFILES+=	bfdver.texi

.include <bsd.info.mk>
