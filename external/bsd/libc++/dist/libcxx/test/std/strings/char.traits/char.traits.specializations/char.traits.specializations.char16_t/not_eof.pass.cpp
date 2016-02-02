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

// static constexpr int_type not_eof(int_type c);

#include <string>
#include <cassert>

int main()
{
#ifndef _LIBCPP_HAS_NO_UNICODE_CHARS
#if __cplusplus >= 201103L
    assert(std::char_traits<char16_t>::not_eof(u'a') == u'a');
    assert(std::char_traits<char16_t>::not_eof(u'A') == u'A');
#endif
    assert(std::char_traits<char16_t>::not_eof(0) == 0);
    assert(std::char_traits<char16_t>::not_eof(std::char_traits<char16_t>::eof()) !=
           std::char_traits<char16_t>::eof());
#endif  // _LIBCPP_HAS_NO_UNICODE_CHARS
}
