//===----------------------------------------------------------------------===//
//
//                     The LLVM Compiler Infrastructure
//
// This file is dual licensed under the MIT and the University of Illinois Open
// Source Licenses. See LICENSE.TXT for details.
//
//===----------------------------------------------------------------------===//

// <valarray>

// template<class T> class valarray;

// valarray& operator^=(const value_type& x);

#include <valarray>
#include <cassert>

int main()
{
    {
        typedef int T;
        T a1[] = { 1,   2,  3,  4,  5};
        T a2[] = { 2,   1,  0,  7,  6};
        const unsigned N = sizeof(a1)/sizeof(a1[0]);
        std::valarray<T> v1(a1, N);
        std::valarray<T> v2(a2, N);
        v1 ^= 3;
        assert(v1.size() == v2.size());
        for (int i = 0; i < v1.size(); ++i)
            assert(v1[i] == v2[i]);
    }
}
