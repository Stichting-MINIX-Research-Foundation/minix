//===----------------------------------------------------------------------===//
//
//                     The LLVM Compiler Infrastructure
//
// This file is dual licensed under the MIT and the University of Illinois Open
// Source Licenses. See LICENSE.TXT for details.
//
//===----------------------------------------------------------------------===//

// <sstream>

// template <class charT, class traits = char_traits<charT>, class Allocator = allocator<charT> >
// class basic_ostringstream

// void str(const basic_string<charT,traits,Allocator>& s);

#include <sstream>
#include <cassert>

int main()
{
    {
        std::ostringstream ss(" 123 456");
        assert(ss.rdbuf() != 0);
        assert(ss.good());
        assert(ss.str() == " 123 456");
        int i = 0;
        ss << i;
        assert(ss.str() == "0123 456");
        ss << 456;
        assert(ss.str() == "0456 456");
        ss.str(" 789");
        assert(ss.str() == " 789");
        ss << "abc";
        assert(ss.str() == "abc9");
    }
    {
        std::wostringstream ss(L" 123 456");
        assert(ss.rdbuf() != 0);
        assert(ss.good());
        assert(ss.str() == L" 123 456");
        int i = 0;
        ss << i;
        assert(ss.str() == L"0123 456");
        ss << 456;
        assert(ss.str() == L"0456 456");
        ss.str(L" 789");
        assert(ss.str() == L" 789");
        ss << L"abc";
        assert(ss.str() == L"abc9");
    }
}
