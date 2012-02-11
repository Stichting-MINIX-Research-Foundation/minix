/*	$NetBSD: SYS.h,v 1.12 2011/01/15 07:31:12 matt Exp $	*/

#include <machine/asm.h>
#include <sys/syscall.h>

#ifdef __STDC__
#define	_DOSYSCALL(x)		li	%r0,(SYS_ ## x)		;\
				sc
#else
#define	_DOSYSCALL(x)		li	%r0,(SYS_/**/x)		;\
				sc
#endif /* __STDC__ */

#define	_SYSCALL_NOERROR(x,y)	.text				;\
				.align	2			;\
			ENTRY(x)				;\
				_DOSYSCALL(y)

#define _SYSCALL(x,y)		.text				;\
				.align	2			;\
			2:	b	_C_LABEL(__cerror)	;\
				_SYSCALL_NOERROR(x,y)		;\
				bso	2b

#define SYSCALL_NOERROR(x)	_SYSCALL_NOERROR(x,x)

#define SYSCALL(x)		_SYSCALL(x,x)

#define PSEUDO_NOERROR(x,y)	_SYSCALL_NOERROR(x,y)		;\
				blr				;\
				END(x)

#define PSEUDO(x,y)		_SYSCALL_NOERROR(x,y)		;\
				bnslr				;\
				b	_C_LABEL(__cerror)	;\
				END(x)

#define RSYSCALL_NOERROR(x)	PSEUDO_NOERROR(x,x)

#define RSYSCALL(x)		PSEUDO(x,x)

#define	WSYSCALL(weak,strong)	WEAK_ALIAS(weak,strong)		;\
				PSEUDO(strong,weak)
