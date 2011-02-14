/*
 * Written by J.T. Conklin <jtc@NetBSD.org>.
 * Public domain.
 */

#include <machine/asm.h>

#if defined(LIBC_SCCS)
	RCSID("$NetBSD: swab.S,v 1.3 2005/08/02 14:20:49 rpaulo Exp $")
#endif

#define LOAD_SWAP_STORE_WORD \
	lodsw	; \
	xchgb	%al,%ah ; \
	stosw

ENTRY(swab)
	xchgq	%rdi,%rsi
	cld				# set direction forward

	shrq	$1,%rdx
	testq	$7,%rdx			# copy first group of 1 to 7 words
	jz	L2			# while swapping alternate bytes.
L1:	lodsw
	rorw	$8,%ax
	stosw
	decq	%rdx
	testq	$7,%rdx
	jnz	L1

L2:	shrq	$3,%rdx			# copy remainder 8 words at a time
	jz	L4			# while swapping alternate bytes.
L3:
	LOAD_SWAP_STORE_WORD
	LOAD_SWAP_STORE_WORD
	LOAD_SWAP_STORE_WORD
	LOAD_SWAP_STORE_WORD
	LOAD_SWAP_STORE_WORD
	LOAD_SWAP_STORE_WORD
	LOAD_SWAP_STORE_WORD
	LOAD_SWAP_STORE_WORD

	decq	%rdx
	jnz	L3
L4:
	ret
