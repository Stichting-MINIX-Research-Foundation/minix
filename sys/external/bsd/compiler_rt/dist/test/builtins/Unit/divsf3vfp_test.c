//===-- divsf3vfp_test.c - Test __divsf3vfp -------------------------------===//
//
//                     The LLVM Compiler Infrastructure
//
// This file is dual licensed under the MIT and the University of Illinois Open
// Source Licenses. See LICENSE.TXT for details.
//
//===----------------------------------------------------------------------===//
//
// This file tests __divsf3vfp for the compiler_rt library.
//
//===----------------------------------------------------------------------===//

#include <stdio.h>
#include <stdlib.h>
#include <math.h>


extern float __divsf3vfp(float a, float b);

#if __arm__
int test__divsf3vfp(float a, float b)
{
    float actual = __divsf3vfp(a, b);
    float expected = a / b;
    if (actual != expected)
        printf("error in test__divsf3vfp(%f, %f) = %f, expected %f\n",
               a, b, actual, expected);
    return actual != expected;
}
#endif

int main()
{
#if __arm__
    if (test__divsf3vfp(1.0, 1.0))
        return 1;
    if (test__divsf3vfp(12345.678, 1.23))
        return 1;
    if (test__divsf3vfp(0.0, HUGE_VALF))
        return 1;
    if (test__divsf3vfp(10.0, -2.0))
        return 1;
#else
    printf("skipped\n");
#endif
    return 0;
}
