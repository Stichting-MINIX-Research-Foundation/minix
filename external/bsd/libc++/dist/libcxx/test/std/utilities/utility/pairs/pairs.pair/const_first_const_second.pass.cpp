//===----------------------------------------------------------------------===//
//
//                     The LLVM Compiler Infrastructure
//
// This file is dual licensed under the MIT and the University of Illinois Open
// Source Licenses. See LICENSE.TXT for details.
//
//===----------------------------------------------------------------------===//

// <utility>

// template <class T1, class T2> struct pair

// pair(const T1& x, const T2& y);

#include <utility>
#include <cassert>

class A
{
    int data_;
public:
    A(int data) : data_(data) {}

    bool operator==(const A& a) const {return data_ == a.data_;}
};

#if _LIBCPP_STD_VER > 11
class AC
{
    int data_;
public:
    constexpr AC(int data) : data_(data) {}

    constexpr bool operator==(const AC& a) const {return data_ == a.data_;}
};
#endif

int main()
{
    {
        typedef std::pair<float, short*> P;
        P p(3.5f, 0);
        assert(p.first == 3.5f);
        assert(p.second == nullptr);
    }
    {
        typedef std::pair<A, int> P;
        P p(1, 2);
        assert(p.first == A(1));
        assert(p.second == 2);
    }

#if _LIBCPP_STD_VER > 11
    {
        typedef std::pair<float, short*> P;
        constexpr P p(3.5f, 0);
        static_assert(p.first == 3.5f, "");
        static_assert(p.second == nullptr, "");
    }
    {
        typedef std::pair<AC, int> P;
        constexpr P p(1, 2);
        static_assert(p.first == AC(1), "");
        static_assert(p.second == 2, "");
    }
#endif
}
