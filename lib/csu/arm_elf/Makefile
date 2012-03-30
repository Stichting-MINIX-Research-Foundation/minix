#	$NetBSD: Makefile,v 1.4 2006/05/19 19:11:12 christos Exp $
CPPFLAGS+= -I${.CURDIR}

# Temporary hack to work around ld problems when linking Thumb applications
# where the linker does not correctly insert an interworking veneer.
CFLAGS+=-mlong-calls

.include "${.CURDIR}/../common_elf/Makefile.inc"
