//===----------------------------------------------------------------------===//
//
//                     The LLVM Compiler Infrastructure
//
// This file is dual licensed under the MIT and the University of Illinois Open
// Source Licenses. See LICENSE.TXT for details.
//
//===----------------------------------------------------------------------===//

// <string>

// constexpr template<class charT, class traits, class Allocator>
//   bool operator>(const charT* lhs, basic_string_wiew<charT,traits> rhs);
// constexpr template<class charT, class traits, class Allocator>
//   bool operator>(basic_string_wiew<charT,traits> lhs, const charT* rhs);

#include <experimental/string_view>
#include <cassert>

#include "constexpr_char_traits.hpp"

template <class S>
void
test(const typename S::value_type* lhs, const S& rhs, bool x, bool y)
{
    assert((lhs > rhs) == x);
    assert((rhs > lhs) == y);
}

int main()
{
    {
    typedef std::experimental::string_view S;
    test("", S(""), false, false);
    test("", S("abcde"), false, true);
    test("", S("abcdefghij"), false, true);
    test("", S("abcdefghijklmnopqrst"), false, true);
    test("abcde", S(""), true, false);
    test("abcde", S("abcde"), false, false);
    test("abcde", S("abcdefghij"), false, true);
    test("abcde", S("abcdefghijklmnopqrst"), false, true);
    test("abcdefghij", S(""), true, false);
    test("abcdefghij", S("abcde"), true, false);
    test("abcdefghij", S("abcdefghij"), false, false);
    test("abcdefghij", S("abcdefghijklmnopqrst"), false, true);
    test("abcdefghijklmnopqrst", S(""), true, false);
    test("abcdefghijklmnopqrst", S("abcde"), true, false);
    test("abcdefghijklmnopqrst", S("abcdefghij"), true, false);
    test("abcdefghijklmnopqrst", S("abcdefghijklmnopqrst"), false, false);
    }

#if _LIBCPP_STD_VER > 11
    {
    typedef std::experimental::basic_string_view<char, constexpr_char_traits<char>> SV;
    constexpr SV  sv1;
    constexpr SV  sv2 { "abcde", 5 };

    static_assert (!(sv1     > ""), "" );
    static_assert (!(""      > sv1), "" );
    static_assert (!(sv1     > "abcde"), "" );
    static_assert (  "abcde" > sv1, "" );
    
    static_assert (  sv2      > "", "" );
    static_assert (!(""       > sv2), "" );
    static_assert (!(sv2      > "abcde"), "" );
    static_assert (!("abcde"  > sv2), "" );
    static_assert (!(sv2      > "abcde0"), "" );
    static_assert (  "abcde0" > sv2, "" );
    }
#endif
}
