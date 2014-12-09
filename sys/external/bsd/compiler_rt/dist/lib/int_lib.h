/* ===-- int_lib.h - configuration header for compiler-rt  -----------------===
 *
 *                     The LLVM Compiler Infrastructure
 *
 * This file is dual licensed under the MIT and the University of Illinois Open
 * Source Licenses. See LICENSE.TXT for details.
 *
 * ===----------------------------------------------------------------------===
 *
 * This file is a configuration header for compiler-rt.
 * This file is not part of the interface of this library.
 *
 * ===----------------------------------------------------------------------===
 */

#ifndef INT_LIB_H
#define INT_LIB_H

/* Assumption: Signed integral is 2's complement. */
/* Assumption: Right shift of signed negative is arithmetic shift. */
/* Assumption: Endianness is little or big (not mixed). */

/* ABI macro definitions */

#if __ARM_EABI__
# define ARM_EABI_FNALIAS(aeabi_name, name)         \
  void __aeabi_##aeabi_name() __attribute__((alias("__" #name)));
# define COMPILER_RT_ABI __attribute__((pcs("aapcs")))
#else
# define ARM_EABI_FNALIAS(aeabi_name, name)
# define COMPILER_RT_ABI
#endif

/* Include the standard compiler builtin headers we use functionality from. */
#include <limits.h>
#include <stdint.h>
#include <stdbool.h>
#include <float.h>

/* Include the commonly used internal type definitions. */
#include "int_types.h"

/* Include internal utility function declarations. */
#include "int_util.h"

#ifdef __minix
/* missing prototypes causing build problems */
COMPILER_RT_ABI di_int __ashrdi3(di_int a, si_int b);
COMPILER_RT_ABI di_int __divdi3(di_int a, di_int b);
COMPILER_RT_ABI di_int __divmoddi4(di_int a, di_int b, di_int* rem);
COMPILER_RT_ABI si_int __divmodsi4(si_int a, si_int b, si_int* rem);
COMPILER_RT_ABI si_int __divsi3(si_int a, si_int b);
COMPILER_RT_ABI si_int __modsi3(si_int a, si_int b);
COMPILER_RT_ABI di_int __lshrdi3(di_int a, si_int b);
COMPILER_RT_ABI di_int __moddi3(di_int a, di_int b);
COMPILER_RT_ABI du_int __udivdi3(du_int a, du_int b);
COMPILER_RT_ABI du_int __udivmoddi4(du_int a, du_int b, du_int* rem);
COMPILER_RT_ABI su_int __udivmodsi4(su_int a, su_int b, su_int* rem);
COMPILER_RT_ABI su_int __udivsi3(su_int n, su_int d);
COMPILER_RT_ABI du_int __umoddi3(du_int a, du_int b);
COMPILER_RT_ABI su_int __umodsi3(su_int a, su_int b);
#endif

#endif /* INT_LIB_H */
