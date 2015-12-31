//===----------------------------------------------------------------------===//
//
//                     The LLVM Compiler Infrastructure
//
// This file is dual licensed under the MIT and the University of Illinois Open
// Source Licenses. See LICENSE.TXT for details.
//
//===----------------------------------------------------------------------===//

// <tgmath.h>

#include <tgmath.h>

#ifndef _LIBCPP_VERSION
#error _LIBCPP_VERSION not defined
#endif

int main()
{
    std::complex<double> cd;
    double x = sin(1.0);
    (void)x; // to placate scan-build
}
