#	$NetBSD: Makefile,v 1.1 2013/11/22 16:00:45 christos Exp $

.include "${.CURDIR}/../../Makefile.inc"

.PATH:	${DIST}/docs/vi.ref

TEXINFO=	vi.texi
INFOFLAGS=	-I${DIST}/doc

vi.info: ref.texi vi.cmd.texi ex.cmd.texi set.opt.texi

.include <bsd.info.mk>
