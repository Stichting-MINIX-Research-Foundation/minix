#	$NetBSD: Makefile,v 1.2 2011/09/25 04:32:46 christos Exp $

PROG=		ar

COPTS.arparse.c = -Wno-stack-protector
CPPFLAGS+=-Dbin_dummy_emulation=bin_vanilla_emulation

.include <bsd.own.mk>

.include "${.CURDIR}/../common/Makefile.prog"
