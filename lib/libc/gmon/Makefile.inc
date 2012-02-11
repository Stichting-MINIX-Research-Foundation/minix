#	$NetBSD: Makefile.inc,v 1.9 2009/12/17 06:54:51 mrg Exp $
#	@(#)Makefile.inc	8.1 (Berkeley) 6/4/93

# gmon sources
.PATH: ${.CURDIR}/gmon ${ARCHDIR}/gmon

.-include "${ARCHDIR}/gmon/Makefile.inc"

SRCS+=	gmon.c mcount.c
MAN+=	moncontrol.3
MLINKS+=moncontrol.3 monstartup.3

.if (${MACHINE_ARCH} == "mipseb") || (${MACHINE_ARCH} == "mipsel") || \
    (${MACHINE_ARCH} == "mips64eb") || (${MACHINE_ARCH} == "mips64el")
# Turn off as(1) warnings on MIPS, since warnings are fatal with WARNS>0
# and mcount.c causes warnings from as(1).
# mcount.c should be fixed and this test removed.
#
COPTS.mcount.c+=-Wa,--no-warn
.endif


# mcount and gmon cannot be compiled with profiling
mcount.po: mcount.o
	cp mcount.o mcount.po

gmon.po: gmon.o
	cp gmon.o gmon.po
