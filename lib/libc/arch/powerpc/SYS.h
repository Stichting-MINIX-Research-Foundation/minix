/*	$NetBSD: SYS.h,v 1.13 2014/08/23 02:24:22 matt Exp $	*/

#include <machine/asm.h>
#include <sys/syscall.h>

#define	BRANCH_TO_CERROR()	b	_C_LABEL(__cerror)

#define	_DOSYSCALL(x)		li	%r0,(SYS_ ## x)		;\
				sc

#define	_SYSCALL_NOERROR(x,y)	.text				;\
				.p2align 2			;\
			ENTRY(x)				;\
				_DOSYSCALL(y)

#define _SYSCALL(x,y)		.text				;\
				.p2align 2			;\
			2:	BRANCH_TO_CERROR()		;\
				_SYSCALL_NOERROR(x,y)		;\
				bso	2b

#define SYSCALL_NOERROR(x)	_SYSCALL_NOERROR(x,x)

#define SYSCALL(x)		_SYSCALL(x,x)

#define PSEUDO_NOERROR(x,y)	_SYSCALL_NOERROR(x,y)		;\
				blr				;\
				END(x)

#define PSEUDO(x,y)		_SYSCALL_NOERROR(x,y)		;\
				bnslr				;\
				BRANCH_TO_CERROR()		;\
				END(x)

#define RSYSCALL_NOERROR(x)	PSEUDO_NOERROR(x,x)

#define RSYSCALL(x)		PSEUDO(x,x)

#define	WSYSCALL(weak,strong)	WEAK_ALIAS(weak,strong)		;\
				PSEUDO(strong,weak)
