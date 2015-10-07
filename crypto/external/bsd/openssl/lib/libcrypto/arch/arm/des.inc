#	$NetBSD: des.inc,v 1.1 2009/07/19 23:30:45 christos Exp $

# XXX WE NEED A WAY TO DETERMINE THESE AT COMPILE-TIME BASED ON
# XXX COMPILER OPTIONS.

# These are good for StrongARM with -mcpu=strongarm
#CPPFLAGS+=	-DDES_RISC1

# These are good for XScale with -mcpu=xscale
#CPPFLAGS+=	-DDES_RISC1 -DDES_PTR

.include "${.CURDIR}/des.inc"
