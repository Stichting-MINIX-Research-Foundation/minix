#	$NetBSD: Makefile,v 1.2 2011/09/25 04:32:47 christos Exp $

PROG=		ranlib

.include <bsd.own.mk>

COPTS.arparse.c = -Wno-stack-protector
CPPFLAGS+=-Dbin_dummy_emulation=bin_vanilla_emulation

.include "${.CURDIR}/../common/Makefile.prog"
