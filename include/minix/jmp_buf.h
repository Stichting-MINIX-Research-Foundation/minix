/* This file is intended for use by assembly language programs that
 * need to manipulate a jmp_buf.  It may only be used by those systems
 * for which a jmp_buf is identical to a struct sigcontext.
 */

#ifndef _JMP_BUF_H
#define _JMP_BUF_H

#if !defined(CHIP)
#include "error, configuration is not known"
#endif

#if (CHIP == INTEL)
#if _WORD_SIZE == 4
#define JB_FLAGS	 0
#define JB_MASK		 4
#define JB_GS		 8
#define JB_FS		10
#define JB_ES		12
#define JB_DS		14
#define JB_DI		16
#define JB_SI		20
#define JB_BP		24
#define JB_ST		28
#define JB_BX		32
#define JB_DX		36
#define JB_CX		40
#define JB_AX		44
#define JB_RETADR	48
#define JB_IP		52
#define JB_CS		56
#define JB_PSW		60
#define JB_SP		64
#define JB_SS		68
#else /* _WORD_SIZE == 2 */
#define JB_FLAGS	 0
#define JB_MASK		 2
#define JB_ES		 6
#define JB_DS		 8 
#define JB_DI		10
#define JB_SI		12
#define JB_BP		14
#define JB_ST		16
#define JB_BX		18
#define JB_DX		20
#define JB_CX		22
#define JB_AX		24
#define JB_RETADR	26
#define JB_IP		28
#define JB_CS		30
#define JB_PSW		32
#define JB_SP		34
#define JB_SS		36
#endif /* _WORD_SIZE == 2 */
#else /* !(CHIP == INTEL) */
#if (CHIP == M68000)
#define JB_FLAGS	 0
#define JB_MASK		 2
#define JB_RETREG	 6
#define JB_D1		10
#define JB_D2		14
#define JB_D3		18
#define JB_D4		22
#define JB_D5		26
#define JB_D6		20
#define JB_D7		34
#define JB_A0		38
#define JB_A1		42
#define JB_A2		46
#define JB_A3		50
#define JB_A4		54
#define JB_A5		58
#define JB_A6		62
#define JB_SP		66
#define JB_PC		70
#define JB_PSW		74
#else /* !(CHIP == INTEL) && !(CHIP == M68000) */
#include "error, CHIP is not supported"
#endif /* (CHIP == INTEL) */

/* Defines from C headers needed in assembly code.  The headers have too
 * much C stuff to used directly.
 */
#define SIG_BLOCK	0	/* must agree with <signal.h> */
#define SC_SIGCONTEXT	2	/* must agree with <sys/sigcontext.h> */
#define SC_NOREGLOCALS	4	/* must agree with <sys/sigcontext.h> */
#endif /* _JMP_BUF_H */
