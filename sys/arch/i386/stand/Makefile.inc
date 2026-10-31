#	$NetBSD: Makefile.inc,v 1.18 2019/01/27 04:56:46 dholland Exp $

NOLIBCSANITIZER=
NOSANITIZER=
NOPIE=
NOMAN=

.include <bsd.own.mk>

BINDIR=	/usr/mdec

OPT_SIZE.gcc=	-Os -ffreestanding -fomit-frame-pointer -fno-unwind-tables \
		-fno-asynchronous-unwind-tables -fno-exceptions -mno-sse
OPT_SIZE.clang=	-Oz -ffreestanding -fomit-frame-pointer -DNDEBUG \
		-fno-stack-protector -mno-sse \
		-mstack-alignment=4 \
		-mllvm -inline-threshold=3 -mllvm -enable-load-pre=false \
		-fno-unwind-tables
