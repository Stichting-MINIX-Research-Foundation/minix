//===----------------------------------------------------------------------===//
//
//                     The LLVM Compiler Infrastructure
//
// This file is dual licensed under the MIT and the University of Illinois Open
// Source Licenses. See LICENSE.TXT for details.
//
//===----------------------------------------------------------------------===//


// <string_view>

// template<class Allocator>
// basic_string_view(const basic_string<_CharT, _Traits, Allocator>& _str) noexcept

#include <experimental/string_view>
#include <string>
#include <cassert>

struct dummy_char_traits : public std::char_traits<char> {};

int main () {
    using string_view = std::experimental::basic_string_view<char>;
    using string      = std::              basic_string     <char, dummy_char_traits>;
    
    {
    string s{"QBCDE"};
    string_view sv1 ( s );
    assert ( sv1.size() == s.size());
    assert ( sv1.data() == s.data());
    }
}