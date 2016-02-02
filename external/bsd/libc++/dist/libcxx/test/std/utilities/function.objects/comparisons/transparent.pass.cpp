//===----------------------------------------------------------------------===//
//
//                     The LLVM Compiler Infrastructure
//
// This file is dual licensed under the MIT and the University of Illinois Open
// Source Licenses. See LICENSE.TXT for details.
//
//===----------------------------------------------------------------------===//

#include <functional>
#include <string>

template <class _Tp>
struct is_transparent
{
private:
    struct __two {char __lx; char __lxx;};
    template <class _Up> static __two __test(...);
    template <class _Up> static char __test(typename _Up::is_transparent* = 0);
public:
    static const bool value = sizeof(__test<_Tp>(0)) == 1;
};


int main () {
#if _LIBCPP_STD_VER > 11

    static_assert ( !is_transparent<std::less<int>>::value, "" );
    static_assert ( !is_transparent<std::less<std::string>>::value, "" );
    static_assert (  is_transparent<std::less<void>>::value, "" );
    static_assert (  is_transparent<std::less<>>::value, "" );

    static_assert ( !is_transparent<std::less_equal<int>>::value, "" );
    static_assert ( !is_transparent<std::less_equal<std::string>>::value, "" );
    static_assert (  is_transparent<std::less_equal<void>>::value, "" );
    static_assert (  is_transparent<std::less_equal<>>::value, "" );

    static_assert ( !is_transparent<std::equal_to<int>>::value, "" );
    static_assert ( !is_transparent<std::equal_to<std::string>>::value, "" );
    static_assert (  is_transparent<std::equal_to<void>>::value, "" );
    static_assert (  is_transparent<std::equal_to<>>::value, "" );

    static_assert ( !is_transparent<std::not_equal_to<int>>::value, "" );
    static_assert ( !is_transparent<std::not_equal_to<std::string>>::value, "" );
    static_assert (  is_transparent<std::not_equal_to<void>>::value, "" );
    static_assert (  is_transparent<std::not_equal_to<>>::value, "" );

    static_assert ( !is_transparent<std::greater<int>>::value, "" );
    static_assert ( !is_transparent<std::greater<std::string>>::value, "" );
    static_assert (  is_transparent<std::greater<void>>::value, "" );
    static_assert (  is_transparent<std::greater<>>::value, "" );

    static_assert ( !is_transparent<std::greater_equal<int>>::value, "" );
    static_assert ( !is_transparent<std::greater_equal<std::string>>::value, "" );
    static_assert (  is_transparent<std::greater_equal<void>>::value, "" );
    static_assert (  is_transparent<std::greater_equal<>>::value, "" );

#endif

    return 0;
    }
