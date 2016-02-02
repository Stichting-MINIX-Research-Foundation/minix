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

// valarray& operator*=(const valarray& v);

#include <valarray>
#include <cassert>

int main()
{
    {
        typedef int T;
        T a1[] = {1,  2,  3,  4,  5};
        T a2[] = {6,  7,  8,  9, 10};
        T a3[] = {6, 14, 24, 36, 50};
        const unsigned N = sizeof(a1)/sizeof(a1[0]);
        std::valarray<T> v1(a1, N);
        std::valarray<T> v2(a2, N);
        std::valarray<T> v3(a3, N);
        v1 *= v2;
        assert(v1.size() == v2.size());
        assert(v1.size() == v3.size());
        for (int i = 0; i < v1.size(); ++i)
            assert(v1[i] == v3[i]);
    }
}
