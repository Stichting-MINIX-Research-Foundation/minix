//===-- negdi2_test.c - Test __negdi2 -------------------------------------===//
//
//                     The LLVM Compiler Infrastructure
//
// This file is dual licensed under the MIT and the University of Illinois Open
// Source Licenses. See LICENSE.TXT for details.
//
//===----------------------------------------------------------------------===//
//
// This file tests __negdi2 for the compiler_rt library.
//
//===----------------------------------------------------------------------===//

#include "int_lib.h"
#include <stdio.h>

// Returns: -a

di_int __negdi2(di_int a);

int test__negdi2(di_int a, di_int expected)
{
    di_int x = __negdi2(a);
    if (x != expected)
        printf("error in __negdi2: -0x%llX = 0x%llX, expected 0x%llX\n",
               a, x, expected);
    return x != expected;
}

char assumption_1[sizeof(di_int) == 2*sizeof(si_int)] = {0};

int main()
{
    if (test__negdi2(0, 0))
        return 1;
    if (test__negdi2(1, -1))
        return 1;
    if (test__negdi2(-1, 1))
        return 1;
    if (test__negdi2(2, -2))
        return 1;
    if (test__negdi2(-2, 2))
        return 1;
    if (test__negdi2(3, -3))
        return 1;
    if (test__negdi2(-3, 3))
        return 1;
    if (test__negdi2(0x00000000FFFFFFFELL, 0xFFFFFFFF00000002LL))
        return 1;
    if (test__negdi2(0xFFFFFFFF00000002LL, 0x00000000FFFFFFFELL))
        return 1;
    if (test__negdi2(0x00000000FFFFFFFFLL, 0xFFFFFFFF00000001LL))
        return 1;
    if (test__negdi2(0xFFFFFFFF00000001LL, 0x00000000FFFFFFFFLL))
        return 1;
    if (test__negdi2(0x0000000100000000LL, 0xFFFFFFFF00000000LL))
        return 1;
    if (test__negdi2(0xFFFFFFFF00000000LL, 0x0000000100000000LL))
        return 1;
    if (test__negdi2(0x0000000200000000LL, 0xFFFFFFFE00000000LL))
        return 1;
    if (test__negdi2(0xFFFFFFFE00000000LL, 0x0000000200000000LL))
        return 1;
    if (test__negdi2(0x0000000300000000LL, 0xFFFFFFFD00000000LL))
        return 1;
    if (test__negdi2(0xFFFFFFFD00000000LL, 0x0000000300000000LL))
        return 1;
    if (test__negdi2(0x8000000000000000LL, 0x8000000000000000LL))
        return 1;
    if (test__negdi2(0x8000000000000001LL, 0x7FFFFFFFFFFFFFFFLL))
        return 1;
    if (test__negdi2(0x7FFFFFFFFFFFFFFFLL, 0x8000000000000001LL))
        return 1;
    if (test__negdi2(0xFFFFFFFE00000000LL, 0x0000000200000000LL))
        return 1;
    if (test__negdi2(0x0000000200000000LL, 0xFFFFFFFE00000000LL))
        return 1;
    if (test__negdi2(0xFFFFFFFF00000000LL, 0x0000000100000000LL))
        return 1;
    if (test__negdi2(0x0000000100000000LL, 0xFFFFFFFF00000000LL))
        return 1;

   return 0;
}
