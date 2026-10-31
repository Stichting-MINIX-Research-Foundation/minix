/*	$NetBSD: linux32_sigcode.S,v 1.6 2018/07/14 14:29:40 maxv Exp $ */

#include "assym.h"
#include <machine/asm.h>

	.code32
ENTRY(linux32_sigcode)
	call	*LINUX32_SF_HANDLER(%esp)
	leal	LINUX32_SF_SC(%esp),%ebx	/* scp */
	pushl	%eax
	movl	$LINUX32_SYS_sigreturn,%eax
	int	$0x80
	movl	$LINUX32_SYS_exit,%eax
	int	$0x80
END(linux32_sigcode)

	.balign	16
ENTRY(linux32_rt_sigcode)
	call	*LINUX32_RT_SF_HANDLER(%esp)
	leal	LINUX32_RT_SF_UC(%esp),%ebx	/* scp */
	pushl	%eax
	movl	$LINUX32_SYS_rt_sigreturn,%eax
	int	$0x80
	movl	$LINUX32_SYS_exit,%eax
	int	$0x80
	.balign	16
	.globl	_C_LABEL(linux32_esigcode)
_C_LABEL(linux32_esigcode):
END(linux32_rt_sigcode)

