/*	$NetBSD: SYS.h,v 1.4 2015/01/12 02:48:20 dennis Exp $	*/

#include <machine/asm.h>
#include <sys/syscall.h>

/*
 * Inline what __cerror() is generally used to do since branching
 * to __cerror() can't be done reliably with the powerpc64 ABI.
 */
#ifdef _REENTRANT
#define	_DO_CERROR_SF_SZ	(SF_SZ + SF_ALIGN(8))
#define	_DO_CERROR()		mflr	%r0				;\
				streg	%r31,-8(%r1)			;\
				streg	%r0,SF_LR(%r1)			;\
				stptru	%r1,-_DO_CERROR_SF_SZ(%r1)	;\
				mr	%r31,%r3			;\
				bl	PIC_PLT(_C_LABEL(__errno))	;\
				nop					;\
				stint	%r31,0(%r3)			;\
				addi	%r1,%r1,_DO_CERROR_SF_SZ	;\
				ldreg	%r0,SF_LR(%r1)			;\
				ldreg	%r31,-8(%r1)			;\
				mtlr	%r0				;\
				li	%r3,-1				;\
				li	%r4,-1				;\
				blr
#else	/* !_REENTRANT */
#define	_DO_CERROR()		lwz	%r4,_C_LABEL(errno)@got(%r2)	;\
				stw	%r3,0(%r4)			;\
				li	%r3,-1				;\
				li	%r4,-1				;\
				blr
#endif	/* _REENTRANT */

/* Clearly BRANCH_TO_CERROR() no longer does that... */
#define	BRANCH_TO_CERROR()	_DO_CERROR()

#define	_DOSYSCALL(x)		li	%r0,(SYS_ ## x)		;\
				sc

#define	_SYSCALL_NOERROR(x,y)	.text				;\
				.p2align 2			;\
			ENTRY(x)				;\
				_DOSYSCALL(y)

#define _SYSCALL(x,y)		.text				;\
				.p2align 2			;\
			2:	_DO_CERROR()			;\
				_SYSCALL_NOERROR(x,y)		;\
				bso	2b

#define SYSCALL_NOERROR(x)	_SYSCALL_NOERROR(x,x)

#define SYSCALL(x)		_SYSCALL(x,x)

#define PSEUDO_NOERROR(x,y)	_SYSCALL_NOERROR(x,y)		;\
				blr				;\
				END(x)

#define PSEUDO(x,y)		_SYSCALL_NOERROR(x,y)		;\
				bnslr				;\
				_DO_CERROR()			;\
				END(x)

#define RSYSCALL_NOERROR(x)	PSEUDO_NOERROR(x,x)

#define RSYSCALL(x)		PSEUDO(x,x)

#define	WSYSCALL(weak,strong)	WEAK_ALIAS(weak,strong)		;\
				PSEUDO(strong,weak)
