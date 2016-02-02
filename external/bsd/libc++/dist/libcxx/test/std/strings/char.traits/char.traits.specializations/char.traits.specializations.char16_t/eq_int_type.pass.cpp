//===----------------------------------------------------------------------===//
//
//                     The LLVM Compiler Infrastructure
//
// This file is dual licensed under the MIT and the University of Illinois Open
// Source Licenses. See LICENSE.TXT for details.
//
//===----------------------------------------------------------------------===//

// <string>

// template<> struct char_traits<char16_t>

// static constexpr bool eq_int_type(int_type c1, int_type c2);

#include <string>
#include <cassert>

int main()
{
#ifndef _LIBCPP_HAS_NO_UNICODE_CHARS
#if __cplusplus >= 201103L
    assert( std::char_traits<char16_t>::eq_int_type(u'a', u'a'));
    assert(!std::char_traits<char16_t>::eq_int_type(u'a', u'A'));
    assert(!std::char_traits<char16_t>::eq_int_type(std::char_traits<char16_t>::eof(), u'A'));
#endif
    assert( std::char_traits<char16_t>::eq_int_type(std::char_traits<char16_t>::eof(),
                                                    std::char_traits<char16_t>::eof()));
#endif  // _LIBCPP_HAS_NO_UNICODE_CHARS
}
