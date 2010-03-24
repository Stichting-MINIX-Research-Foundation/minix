/* Definitions for compiler-specific features. */

#ifndef _MINIX_COMPILER_H
#define _MINIX_COMPILER_H

/*===========================================================================*
 *			      Compiler overrides			     *
 *===========================================================================*/
/* ACK */
#ifdef __ACK__
#include <minix/compiler-ack.h>
#endif

/*===========================================================================*
 *				Default values				     *
 *===========================================================================*/
/*
 * cdecl calling convention expects the callee to pop the hidden pointer on
 * struct return. For example, GCC and LLVM comply with this (tested on IA32).
 */
#ifndef BYTES_TO_POP_ON_STRUCT_RETURN
#define BYTES_TO_POP_ON_STRUCT_RETURN $4
#endif

#endif /* _MINIX_COMPILER_H */
