/* Definitions for ACK-specific features. */

#ifndef _MINIX_COMPILER_ACK_H
#define _MINIX_COMPILER_ACK_H

/* ACK expects the caller to pop the hidden pointer on struct return. */
#define BYTES_TO_POP_ON_STRUCT_RETURN

/* 
 * ACK doesn't move the last argument of a variadic arguments function
 * anywhere, once it's on the stack as a function parameter. Thus, it is
 * possible to make strong assumption on the immutability of the stack
 * layout and use the address of that argument as the start of an array.
 * 
 * If you're curious, just look at lib/libc/posix/_execl*.c ;-)
 */

#define FUNC_ARGS_ARRAY 1

#endif /* _MINIX_COMPILER_ACK_H */
