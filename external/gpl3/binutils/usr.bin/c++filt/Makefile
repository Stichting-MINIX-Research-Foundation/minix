#	$NetBSD: Makefile,v 1.1 2009/08/18 20:22:08 skrll Exp $

PROG=		c++filt
CPPFLAGS+=	-DMAIN
CLEANFILES+=	c++filt.1

.include <bsd.own.mk>

.include "${.CURDIR}/../common/Makefile.prog"

.PATH: ${DIST}/libiberty

c++filt.1: ${DIST}/binutils/doc/cxxfilt.man
	${_MKTARGET_CREATE}
	${TOOL_SED} -e "s/@PROGRAM@/${PROG}/" < $> > $@
