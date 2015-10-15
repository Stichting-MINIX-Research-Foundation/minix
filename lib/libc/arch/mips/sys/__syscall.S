/*	$NetBSD: __syscall.S,v 1.2 2014/01/31 18:06:26 matt Exp $	*/

#include "SYS.h"

#ifdef __mips_o32
RSYSCALL(__syscall)
#else

LEAF(__syscall)
        PIC_PROLOGUE(__syscall)
	move	v0, a0
	move	a0, a1
	move	a1, a2
	move	a2, a3
	move	a3, a4
	move	a4, a5
	move	a5, a6
	move	a6, a7
	REG_L	a7, (sp)
        SYSTRAP(__syscall)
        bne a3,zero,1f
        PIC_RETURN()
1:
        PIC_TAILCALL(__cerror)
END(__syscall)
STRONG_ALIAS(syscall, __syscall)
#endif
