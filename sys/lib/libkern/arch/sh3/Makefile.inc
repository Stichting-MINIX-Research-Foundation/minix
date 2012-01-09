#	$NetBSD: Makefile.inc,v 1.19 2011/08/04 03:20:09 uwe Exp $

SRCS+=	ffs.S
SRCS+=	memset.S
SRCS+=	memmove.S memcpy.S byte_swap_2.S byte_swap_4.S byte_swap_8.S
SRCS+=	ashiftrt.S ashlsi3.S ashrsi3.S lshrsi3.S movstr.S movstr_i4.S
SRCS+=	movstrSI.S movstrSI12_i4.S mulsi3.S sdivsi3.S udivsi3.S

# newer gcc uses different integer division millicode by default
SRCS+=  sdivsi3_i4i.S udivsi3_i4i.S

NO_SRCS+= bswap64.c
