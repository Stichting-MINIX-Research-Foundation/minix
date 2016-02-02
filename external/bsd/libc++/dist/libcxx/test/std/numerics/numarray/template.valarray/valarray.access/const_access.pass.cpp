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

// const value_type& operator[](size_t i) const;

#include <valarray>
#include <cassert>

int main()
{
    {
        typedef int T;
        T a[] = {5, 4, 3, 2, 1};
        const unsigned N = sizeof(a)/sizeof(a[0]);
        const std::valarray<T> v(a, N);
        for (int i = 0; i < N; ++i)
        {
            assert(v[i] == a[i]);
        }
    }
}
