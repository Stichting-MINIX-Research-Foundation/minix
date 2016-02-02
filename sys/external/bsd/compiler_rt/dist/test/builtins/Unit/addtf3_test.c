//===--------------- addtf3_test.c - Test __addtf3 ------------------------===//
//
//                     The LLVM Compiler Infrastructure
//
// This file is dual licensed under the MIT and the University of Illinois Open
// Source Licenses. See LICENSE.TXT for details.
//
//===----------------------------------------------------------------------===//
//
// This file tests __addtf3 for the compiler_rt library.
//
//===----------------------------------------------------------------------===//

#include <stdio.h>

#if __LDBL_MANT_DIG__ == 113

#include "fp_test.h"

// Returns: a + b
long double __addtf3(long double a, long double b);

int test__addtf3(long double a, long double b,
                 uint64_t expectedHi, uint64_t expectedLo)
{
    long double x = __addtf3(a, b);
    int ret = compareResultLD(x, expectedHi, expectedLo);

    if (ret){
        printf("error in test__addtf3(%.20Lf, %.20Lf) = %.20Lf, "
               "expected %.20Lf\n", a, b, x,
               fromRep128(expectedHi, expectedLo));
    }

    return ret;
}

char assumption_1[sizeof(long double) * CHAR_BIT == 128] = {0};

#endif

int main()
{
#if __LDBL_MANT_DIG__ == 113
    // qNaN + any = qNaN
    if (test__addtf3(makeQNaN128(),
                     0x1.23456789abcdefp+5L,
                     UINT64_C(0x7fff800000000000),
                     UINT64_C(0x0)))
        return 1;
    // NaN + any = NaN
    if (test__addtf3(makeNaN128(UINT64_C(0x800030000000)),
                     0x1.23456789abcdefp+5L,
                     UINT64_C(0x7fff800000000000),
                     UINT64_C(0x0)))
        return 1;
    // inf + inf = inf
    if (test__addtf3(makeInf128(),
                     makeInf128(),
                     UINT64_C(0x7fff000000000000),
                     UINT64_C(0x0)))
        return 1;
    // inf + any = inf
    if (test__addtf3(makeInf128(),
                     0x1.2335653452436234723489432abcdefp+5L,
                     UINT64_C(0x7fff000000000000),
                     UINT64_C(0x0)))
        return 1;
    // any + any
    if (test__addtf3(0x1.23456734245345543849abcdefp+5L,
                     0x1.edcba52449872455634654321fp-1L,
                     UINT64_C(0x40042afc95c8b579),
                     UINT64_C(0x61e58dd6c51eb77c)))
        return 1;

#else
    printf("skipped\n");

#endif
    return 0;
}
