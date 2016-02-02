//===----------------------------------------------------------------------===//
//
//                     The LLVM Compiler Infrastructure
//
// This file is dual licensed under the MIT and the University of Illinois Open
// Source Licenses. See LICENSE.TXT for details.
//
//===----------------------------------------------------------------------===//
//
// This test uses new symbols that were not defined in the libc++ shipped on
// darwin11 and darwin12:
// XFAIL: with_system_cxx_lib=x86_64-apple-darwin11
// XFAIL: with_system_cxx_lib=x86_64-apple-darwin12

// <locale>

// typedef int category;

#include <locale>
#include <type_traits>
#include <cassert>

template <class _Tp>
void test(const _Tp &) {}


int main()
{
    static_assert((std::is_same<std::locale::category, int>::value), "");
    assert(std::locale::none == 0);
    assert(std::locale::collate);
    assert(std::locale::ctype);
    assert(std::locale::monetary);
    assert(std::locale::numeric);
    assert(std::locale::time);
    assert(std::locale::messages);
    assert((std::locale::collate
          & std::locale::ctype
          & std::locale::monetary
          & std::locale::numeric
          & std::locale::time
          & std::locale::messages) == 0);
    assert((std::locale::collate
          | std::locale::ctype
          | std::locale::monetary
          | std::locale::numeric
          | std::locale::time
          | std::locale::messages)
         == std::locale::all);

    test(std::locale::none);
    test(std::locale::collate);
    test(std::locale::ctype);
    test(std::locale::monetary);
    test(std::locale::numeric);
    test(std::locale::time);
    test(std::locale::messages);
    test(std::locale::all);
}
