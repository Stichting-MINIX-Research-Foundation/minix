//===----------------------------------------------------------------------===//
//
//                     The LLVM Compiler Infrastructure
//
// This file is dual licensed under the MIT and the University of Illinois Open
// Source Licenses. See LICENSE.TXT for details.
//
//===----------------------------------------------------------------------===//

// <random>

// template<class RealType = double>
// class weibull_distribution

// result_type min() const;

#include <random>
#include <cassert>

int main()
{
    {
        typedef std::weibull_distribution<> D;
        D d(.5, .5);
        assert(d.min() == 0);
    }
}
