//===-- addvsi3_test.c - Test __addvsi3 -----------------------------------===//
//
//                     The LLVM Compiler Infrastructure
//
// This file is dual licensed under the MIT and the University of Illinois Open
// Source Licenses. See LICENSE.TXT for details.
//
//===----------------------------------------------------------------------===//
//
// This file tests __addvsi3 for the compiler_rt library.
//
//===----------------------------------------------------------------------===//

#include "int_lib.h"
#include <stdio.h>

// Returns: a + b

// Effects: aborts if a + b overflows

si_int __addvsi3(si_int a, si_int b);

int test__addvsi3(si_int a, si_int b)
{
    si_int x = __addvsi3(a, b);
    si_int expected = a + b;
    if (x != expected)
        printf("error in test__addvsi3(0x%X, 0x%X) = %d, expected %d\n",
               a, b, x, expected);
    return x != expected;
}

int main()
{
//     test__addvsi3(0x80000000, -1);  // should abort
//     test__addvsi3(-1, 0x80000000);  // should abort
//     test__addvsi3(1, 0x7FFFFFFF);  // should abort
//     test__addvsi3(0x7FFFFFFF, 1);  // should abort

    if (test__addvsi3(0x80000000, 1))
        return 1;
    if (test__addvsi3(1, 0x80000000))
        return 1;
    if (test__addvsi3(0x80000000, 0))
        return 1;
    if (test__addvsi3(0, 0x80000000))
        return 1;
    if (test__addvsi3(0x7FFFFFFF, -1))
        return 1;
    if (test__addvsi3(-1, 0x7FFFFFFF))
        return 1;
    if (test__addvsi3(0x7FFFFFFF, 0))
        return 1;
    if (test__addvsi3(0, 0x7FFFFFFF))
        return 1;

    return 0;
}
