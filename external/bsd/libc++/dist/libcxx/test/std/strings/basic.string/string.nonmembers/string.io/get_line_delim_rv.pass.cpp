//===----------------------------------------------------------------------===//
//
//                     The LLVM Compiler Infrastructure
//
// This file is dual licensed under the MIT and the University of Illinois Open
// Source Licenses. See LICENSE.TXT for details.
//
//===----------------------------------------------------------------------===//

// <string>

// template<class charT, class traits, class Allocator>
//   basic_istream<charT,traits>&
//   getline(basic_istream<charT,traits>&& is,
//           basic_string<charT,traits,Allocator>& str, charT delim);

#include <string>
#include <sstream>
#include <cassert>

#include "min_allocator.h"

int main()
{
#ifndef _LIBCPP_HAS_NO_RVALUE_REFERENCES
    {
        std::string s("initial text");
        getline(std::istringstream(" abc*  def*   ghij"), s, '*');
        assert(s == " abc");
    }
    {
        std::wstring s(L"initial text");
        getline(std::wistringstream(L" abc*  def*   ghij"), s, L'*');
        assert(s == L" abc");
    }
#if __cplusplus >= 201103L
    {
        typedef std::basic_string<char, std::char_traits<char>, min_allocator<char>> S;
        S s("initial text");
        getline(std::istringstream(" abc*  def*   ghij"), s, '*');
        assert(s == " abc");
    }
    {
        typedef std::basic_string<wchar_t, std::char_traits<wchar_t>, min_allocator<wchar_t>> S;
        S s(L"initial text");
        getline(std::wistringstream(L" abc*  def*   ghij"), s, L'*');
        assert(s == L" abc");
    }
#endif
#endif  // _LIBCPP_HAS_NO_RVALUE_REFERENCES
}
