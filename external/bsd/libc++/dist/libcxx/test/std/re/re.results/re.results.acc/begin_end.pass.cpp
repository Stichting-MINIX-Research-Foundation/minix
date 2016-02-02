//===----------------------------------------------------------------------===//
//
//                     The LLVM Compiler Infrastructure
//
// This file is dual licensed under the MIT and the University of Illinois Open
// Source Licenses. See LICENSE.TXT for details.
//
//===----------------------------------------------------------------------===//

// <regex>

// class match_results<BidirectionalIterator, Allocator>

// const_iterator begin() const;
// const_iterator end() const;

#include <regex>
#include <cassert>

void
test()
{
    std::match_results<const char*> m;
    const char s[] = "abcdefghijk";
    assert(std::regex_search(s, m, std::regex("cd((e)fg)hi")));

    std::match_results<const char*>::const_iterator i = m.begin();
    std::match_results<const char*>::const_iterator e = m.end();

    assert(e - i == m.size());
    for (int j = 0; i != e; ++i, ++j)
        assert(*i == m[j]);
}

int main()
{
    test();
}
