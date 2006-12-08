/* This file is intended for use by program code (possibly written in
 * assembly) that needs to manipulate a jmp_buf or sigjmp_buf. The JB_*
 * values are byte offsets into the jmp_buf and sigjmp_buf structures.
 */

#ifndef _JMP_BUF_H
#define _JMP_BUF_H

#include <minix/config.h>

#if defined(__ACK__)
/* as per lib/ack/rts/setjmp.e */

/* note the lack of parentheses, which would confuse 'as' */
#define JB_PC		0
#define JB_SP		JB_PC + _EM_PSIZE
#define JB_LB		JB_SP + _EM_PSIZE
#define JB_MASK		JB_LB + _EM_PSIZE
#define JB_FLAGS	JB_MASK + _EM_LSIZE

#if (CHIP == INTEL)
#define JB_BP		JB_LB
#endif

#elif defined(__GNUC__)

#if (CHIP == INTEL) && (_WORD_SIZE == 4)
/* as per lib/gnu/rts/__setjmp.gs */

#define JB_FLAGS	0
#define JB_MASK		4
#define JB_PC		8
#define JB_SP		12
#define JB_BP		16
#define JB_BX		20
#define JB_CX		24
#define JB_DX		28
#define JB_SI		32
#define JB_DI		36

#endif /* (CHIP == INTEL) && (_WORD_SIZE == 4) */

#endif /* __GNU__ */

#endif /* _JMP_BUF_H */
