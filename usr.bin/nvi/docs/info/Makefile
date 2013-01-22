#	$NetBSD: Makefile,v 1.2 2008/12/09 23:49:42 lukem Exp $

.include "${.CURDIR}/../../Makefile.inc"

.PATH:	${DIST}/docs/vi.ref

TEXINFO=	vi.texi
INFOFLAGS=	-I${DIST}/doc

vi.info: ref.texi vi.cmd.texi ex.cmd.texi set.opt.texi

.include <bsd.info.mk>
