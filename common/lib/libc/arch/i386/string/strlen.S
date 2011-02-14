/*
 * Written by J.T. Conklin <jtc@acorntoolworks.com>
 * Public domain.
 */

#include <machine/asm.h>

#if defined(LIBC_SCCS)
	RCSID("$NetBSD: strlen.S,v 1.1 2005/12/20 19:28:49 christos Exp $")
#endif

ENTRY(strlen)
	movl	4(%esp),%eax

.Lalign:
	/* Consider unrolling loop? */
	testb	$3,%al
	je	.Lword_aligned
	cmpb	$0,(%eax)
	je	.Ldone
	incl	%eax
	jmp	.Lalign

	/*
	 * There are many well known branch-free sequences which are used
	 * for determining whether a zero-byte is contained within a word.
	 * These sequences are generally much more efficent than loading
	 * and comparing each byte individually.
	 *
	 * The expression [1,2]:
	 *
	 * (1)  ~(((x & 0x7f7f7f7f) + 0x7f7f7f7f) | (x | 0x7f7f7f7f))
	 *
	 * evaluates to a non-zero value if any of the bytes in the
	 * original word is zero.
	 *
	 * It also has the useful property that bytes in the result word
	 * that correspond to non-zero bytes in the original word have
	 * the value 0x00, while bytes corresponding to zero bytes have
	 * the value 0x80. This allows calculation of the first (and
	 * last) occurrence of a zero byte within the word (useful for C's
	 * str* primitives) by counting the number of leading (or
	 * trailing) zeros and dividing the result by 8.  On machines
	 * without (or with slow) clz() / ctz() instructions, testing
	 * each byte in the result word for zero is necessary.
	 *
	 * This typically takes 4 instructions (5 on machines without
	 * "not-or") not including those needed to load the constant.
	 *
	 *
	 * The expression:
	 *
	 * (2)  ((x - 0x01010101) & ~x & 0x80808080)
	 *
	 * evaluates to a non-zero value if any of the bytes in the
	 * original word is zero.
	 *
	 * On little endian machines, the first byte in the result word
	 * that corresponds to a zero byte in the original byte is 0x80,
	 * so clz() can be used as above.  On big endian machines, and
	 * little endian machines without (or with a slow) clz() insn,
	 * testing each byte in the original for zero is necessary.
	 *
	 * This typically takes 3 instructions (4 on machines without
	 * "and with complement") not including those needed to load
	 * constants.
	 *
	 *
	 * The expression:
	 *
	 * (3)  ((x - 0x01010101) & 0x80808080)
	 *
	 * always evaluates to a non-zero value if any of the bytes in
	 * the original word is zero.  However, in rare cases, it also
	 * evaluates to a non-zero value when none of the bytes in the
	 * original word is zero.
	 *
	 * To account for possible false positives, each byte of the
	 * original word must be checked when the expression evaluates to
	 * a non-zero value.  However, because it is simpler than those
	 * presented above, code that uses it will be faster as long as
	 * the rate of false positives is low.
	 *
	 * This is likely, because the the false positive can only occur
	 * if the most siginificant bit of a byte within the word is set.
	 * The expression will never fail for typical 7-bit ASCII strings.
	 *
	 * This typically takes 2 instructions not including those needed
	 * to load constants.
	 *
	 *
	 * [1] Henry S. Warren Jr., "Hacker's Delight", Addison-Westley 2003
	 *
	 * [2] International Business Machines, "The PowerPC Compiler Writer's
	 *     Guide", Warthman Associates, 1996
	 */

	_ALIGN_TEXT
.Lword_aligned:
.Lloop:
	movl	(%eax),%ecx
	addl	$4,%eax
	leal	-0x01010101(%ecx),%edx
	testl	$0x80808080,%edx
	je	.Lloop

	/*
	 * In rare cases, the above loop may exit prematurely. We must
	 * return to the loop if none of the bytes in the word equal 0.
	 */

	/*
	 * The optimal code for determining whether each byte is zero
	 * differs by processor.  This space-optimized code should be
	 * acceptable on all, especially since we don't expect it to
	 * be run frequently,
	 */

	testb	%cl,%cl		/* 1st byte == 0? */
	jne	1f
	subl	$4,%eax
	jmp	.Ldone

1:	testb	%ch,%ch		/* 2nd byte == 0? */
	jne	1f
	subl	$3,%eax
	jmp	.Ldone

1:	shrl	$16,%ecx
	testb	%cl,%cl		/* 3rd byte == 0? */
	jne	1f
	subl	$2,%eax
	jmp	.Ldone

1:	testb	%ch,%ch		/* 4th byte == 0? */
	jne	.Lloop
	decl	%eax

.Ldone:
	subl	4(%esp),%eax
	ret
