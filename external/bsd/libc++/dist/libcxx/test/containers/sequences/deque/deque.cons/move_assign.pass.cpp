//===----------------------------------------------------------------------===//
//
//                     The LLVM Compiler Infrastructure
//
// This file is dual licensed under the MIT and the University of Illinois Open
// Source Licenses. See LICENSE.TXT for details.
//
//===----------------------------------------------------------------------===//

// <deque>

// deque& operator=(deque&& c);

#include <deque>
#include <cassert>

#include "../../../MoveOnly.h"
#include "test_allocator.h"
#include "min_allocator.h"

int main()
{
#ifndef _LIBCPP_HAS_NO_RVALUE_REFERENCES
    {
        int ab[] = {3, 4, 2, 8, 0, 1, 44, 34, 45, 96, 80, 1, 13, 31, 45};
        int* an = ab + sizeof(ab)/sizeof(ab[0]);
        typedef test_allocator<MoveOnly> A;
        std::deque<MoveOnly, A> c1(A(5));
        for (int* p = ab; p < an; ++p)
            c1.push_back(MoveOnly(*p));
        std::deque<MoveOnly, A> c2(A(5));
        for (int* p = ab; p < an; ++p)
            c2.push_back(MoveOnly(*p));
        std::deque<MoveOnly, A> c3(A(5));
        c3 = std::move(c1);
        assert(c2 == c3);
        assert(c1.size() == 0);
        assert(c3.get_allocator() == A(5));
    }
    {
        int ab[] = {3, 4, 2, 8, 0, 1, 44, 34, 45, 96, 80, 1, 13, 31, 45};
        int* an = ab + sizeof(ab)/sizeof(ab[0]);
        typedef test_allocator<MoveOnly> A;
        std::deque<MoveOnly, A> c1(A(5));
        for (int* p = ab; p < an; ++p)
            c1.push_back(MoveOnly(*p));
        std::deque<MoveOnly, A> c2(A(5));
        for (int* p = ab; p < an; ++p)
            c2.push_back(MoveOnly(*p));
        std::deque<MoveOnly, A> c3(A(6));
        c3 = std::move(c1);
        assert(c2 == c3);
        assert(c1.size() != 0);
        assert(c3.get_allocator() == A(6));
    }
    {
        int ab[] = {3, 4, 2, 8, 0, 1, 44, 34, 45, 96, 80, 1, 13, 31, 45};
        int* an = ab + sizeof(ab)/sizeof(ab[0]);
        typedef other_allocator<MoveOnly> A;
        std::deque<MoveOnly, A> c1(A(5));
        for (int* p = ab; p < an; ++p)
            c1.push_back(MoveOnly(*p));
        std::deque<MoveOnly, A> c2(A(5));
        for (int* p = ab; p < an; ++p)
            c2.push_back(MoveOnly(*p));
        std::deque<MoveOnly, A> c3(A(6));
        c3 = std::move(c1);
        assert(c2 == c3);
        assert(c1.size() == 0);
        assert(c3.get_allocator() == A(5));
    }
#if __cplusplus >= 201103L
    {
        int ab[] = {3, 4, 2, 8, 0, 1, 44, 34, 45, 96, 80, 1, 13, 31, 45};
        int* an = ab + sizeof(ab)/sizeof(ab[0]);
        typedef min_allocator<MoveOnly> A;
        std::deque<MoveOnly, A> c1(A{});
        for (int* p = ab; p < an; ++p)
            c1.push_back(MoveOnly(*p));
        std::deque<MoveOnly, A> c2(A{});
        for (int* p = ab; p < an; ++p)
            c2.push_back(MoveOnly(*p));
        std::deque<MoveOnly, A> c3(A{});
        c3 = std::move(c1);
        assert(c2 == c3);
        assert(c1.size() == 0);
        assert(c3.get_allocator() == A());
    }
#endif
#endif  // _LIBCPP_HAS_NO_RVALUE_REFERENCES
}
