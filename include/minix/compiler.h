/* Definitions for compiler-specific features. */

#ifndef _MINIX_COMPILER_H
#define _MINIX_COMPILER_H

/*===========================================================================*
 *                          Compiler overrides                               *
 *===========================================================================*/
/* ACK */
#ifdef __ACK__
#include <minix/compiler-ack.h>
#endif

/*===========================================================================*
 *                             Default values                                *
 *===========================================================================*/
/*
 * cdecl calling convention expects the callee to pop the hidden pointer on
 * struct return. For example, GCC and LLVM comply with this (tested on IA32).
 */
#ifndef BYTES_TO_POP_ON_STRUCT_RETURN
#define BYTES_TO_POP_ON_STRUCT_RETURN $4
#endif

/*
 * cdecl calling convention requires to push arguments on the stack in a 
 * reverse order to easily support variadic arguments. Thus, instead of
 * using the proper stdarg.h macros (that nowadays are
 * compiler-dependant), it may be tempting to directly take the address of 
 * the last argument and considering it as the start of an array. This is
 * a shortcut that avoid looping to get all the arguments as the CPU
 * already pushed them on the stack before the call to the function.
 * 
 * Unfortunately, such an assumption is strictly compiler-dependant and
 * compilers are free to move the last argument on the stack, as a local
 * variable, and return the address of the location where the argument was
 * stored, if asked for. This will break things as the rest of the array's
 * argument are stored elsewhere (typically, a couple of words above the
 * location where the argument was stored).
 *
 * Conclusion: if unsure on what the compiler may do, do not make any 
 * assumption and use the right (typically compiler-dependant) macros. 
 */

#ifndef FUNC_ARGS_ARRAY
#define FUNC_ARGS_ARRAY 0
#endif

#endif /* _MINIX_COMPILER_H */
