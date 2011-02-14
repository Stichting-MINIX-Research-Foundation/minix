/*
 * Written by J.T. Conklin <jtc@NetBSD.org>.
 * Public domain.
 */

#include <machine/asm.h>

#if defined(LIBC_SCCS)
	RCSID("$NetBSD: memcmp.S,v 1.2 2007/11/12 18:41:59 ad Exp $")
#endif

ENTRY(memcmp)
	pushl	%edi
	pushl	%esi
	movl	12(%esp),%edi
	movl	16(%esp),%esi

	movl	20(%esp),%ecx		/* compare by words */
	shrl	$2,%ecx
	repe
	cmpsl
	jne	L5			/* do we match so far? */

	movl	20(%esp),%ecx		/* compare remainder by bytes */
	andl	$3,%ecx
	repe
	cmpsb
	jne	L6			/* do we match? */

	xorl	%eax,%eax		/* we match, return zero	*/
	popl	%esi
	popl	%edi
	ret

L5:	movl	$4,%ecx			/* We know that one of the next	*/
	subl	%ecx,%edi		/* four pairs of bytes do not	*/
	subl	%ecx,%esi		/* match.			*/
	repe
	cmpsb
L6:	xorl	%eax,%eax		/* Perform unsigned comparison	*/
	movb	-1(%edi),%al
	xorl	%edx,%edx
	movb	-1(%esi),%dl
	subl    %edx,%eax
	popl	%esi
	popl	%edi
	ret
