#	$NetBSD: Makefile,v 1.13 2020/06/21 17:17:01 thorpej Exp $

.include <bsd.hostinit.mk>

HOSTPROGNAME=	${_TOOL_PREFIX}installboot
HOST_SRCDIR=	usr.sbin/installboot

LIBPROP_INC=	${.CURDIR}/../../common/include
HOST_CPPFLAGS+=	-I${LIBPROP_INC}

HOST_CPPFLAGS+=	-I. -I${.CURDIR} -I${.CURDIR}/../mips-elf2ecoff
HOST_CPPFLAGS+=	-I${TOOLDIR}/include/nbinclude

HOST_CPPFLAGS+=	-DEVBOARDS_PLIST_BASE=\"${TOOLDIR}\"

LIBPROP_OBJ!=	cd ${.CURDIR}/../libprop && ${PRINTOBJDIR}
LDADD+=		-L${LIBPROP_OBJ} -lprop

SHARE_SRCDIR=	${.CURDIR}/../../share
HOST_SHAREDIR=	${TOOLDIR}/share

BOARDDB_SRCDIR=	${SHARE_SRCDIR}/installboot
BOARDDB_DSTDIR=	${HOST_SHAREDIR}/installboot

BOARDDBS=	evbarm evbmips

.for _d in ${BOARDDBS}
install: .PHONY install.${_d}.boards.plist
install.${_d}.boards.plist: .PHONY ${BOARDDB_DSTDIR}/${_d}/boards.plist
${BOARDDB_DSTDIR}/${_d}/boards.plist: ${BOARDDB_SRCDIR}/${_d}/boards.plist
	${_MKTARGET_INSTALL}
	${HOST_INSTALL_DIR} ${HOST_SHAREDIR}
	${HOST_INSTALL_DIR} ${BOARDDB_DSTDIR}
	${HOST_INSTALL_DIR} ${BOARDDB_DSTDIR}/${_d}
	${HOST_INSTALL_FILE} -m ${NONBINMODE} ${.ALLSRC} ${.TARGET}
.endfor

.include "${.CURDIR}/../Makefile.nbincludes"
.include "${.CURDIR}/../Makefile.host"

