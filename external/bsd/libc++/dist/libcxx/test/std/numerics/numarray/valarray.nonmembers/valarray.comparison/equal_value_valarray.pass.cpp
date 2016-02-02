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

// template<class T>
//   valarray<bool>
//   operator==(const T& x, const valarray<T>& y);

#include <valarray>
#include <cassert>

int main()
{
    {
        typedef int T;
        T a2[] = {1,  2,  3,  4,  0};
        bool a3[] = {false,  true,  false,  false,  false};
        const unsigned N = sizeof(a2)/sizeof(a2[0]);
        std::valarray<T> v2(a2, N);
        std::valarray<bool> v3 = 2 == v2;
        assert(v2.size() == v3.size());
        for (int i = 0; i < v3.size(); ++i)
            assert(v3[i] == a3[i]);
    }
}
