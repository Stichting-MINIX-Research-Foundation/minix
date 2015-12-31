//===----------------------------------------------------------------------===//
//
//                     The LLVM Compiler Infrastructure
//
// This file is dual licensed under the MIT and the University of Illinois Open
// Source Licenses. See LICENSE.TXT for details.
//
//===----------------------------------------------------------------------===//

// <optional>

// constexpr const T& optional<T>::operator*() const;

#ifdef _LIBCPP_DEBUG
#define _LIBCPP_ASSERT(x, m) ((x) ? (void)0 : std::exit(0))
#endif

#include <experimental/optional>
#include <type_traits>
#include <cassert>

#if _LIBCPP_STD_VER > 11

using std::experimental::optional;

struct X
{
    constexpr int test() const {return 3;}
};

struct Y
{
    int test() const {return 2;}
};

#endif  // _LIBCPP_STD_VER > 11

int main()
{
#if _LIBCPP_STD_VER > 11
    {
        constexpr optional<X> opt(X{});
        static_assert((*opt).test() == 3, "");
    }
    {
        constexpr optional<Y> opt(Y{});
        assert((*opt).test() == 2);
    }
#ifdef _LIBCPP_DEBUG
    {
        const optional<X> opt;
        assert((*opt).test() == 3);
        assert(false);
    }
#endif  // _LIBCPP_DEBUG
#endif  // _LIBCPP_STD_VER > 11
}
