/*	$NetBSD: edk2asm.h,v 1.1.1.1 2018/08/16 18:17:47 jmcneill Exp $	*/


#define ASM_PFX(x)			x
#define GCC_ASM_EXPORT(x)		\
	.globl		x		; \
	.type		x, %function

