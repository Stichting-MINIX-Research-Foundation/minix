//===----------------------------------------------------------------------===//
//
//                     The LLVM Compiler Infrastructure
//
// This file is dual licensed under the MIT and the University of Illinois Open
// Source Licenses. See LICENSE.TXT for details.
//
//===----------------------------------------------------------------------===//

// <string>

// template<> struct char_traits<wchar_t>

// static constexpr bool eq(char_type c1, char_type c2);

#include <string>
#include <cassert>

int main()
{
    wchar_t c = L'\0';
    assert(std::char_traits<wchar_t>::eq(L'a', L'a'));
    assert(!std::char_traits<wchar_t>::eq(L'a', L'A'));
}
