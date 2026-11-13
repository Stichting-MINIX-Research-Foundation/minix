/*	$NetBSD: efisetjmp.h,v 1.1.1.1 2018/08/16 18:17:47 jmcneill Exp $	*/

#ifndef GNU_EFI_SETJMP_H
#define GNU_EFI_SETJMP_H

#include "eficompiler.h"
#include "efisetjmp_arch.h"

extern UINTN setjmp(jmp_buf *env) __attribute__((returns_twice));
extern VOID longjmp(jmp_buf *env, UINTN value) __attribute__((noreturn));

#endif /* GNU_EFI_SETJMP_H */
