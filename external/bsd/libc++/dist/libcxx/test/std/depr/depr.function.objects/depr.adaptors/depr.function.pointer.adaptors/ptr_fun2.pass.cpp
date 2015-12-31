//===----------------------------------------------------------------------===//
//
//                     The LLVM Compiler Infrastructure
//
// This file is dual licensed under the MIT and the University of Illinois Open
// Source Licenses. See LICENSE.TXT for details.
//
//===----------------------------------------------------------------------===//

// <functional>

// template <CopyConstructible Arg1, CopyConstructible Arg2, Returnable Result>
// pointer_to_binary_function<Arg1,Arg2,Result>
// ptr_fun(Result (*f)(Arg1, Arg2));

#include <functional>
#include <type_traits>
#include <cassert>

double binary_f(int i, short j) {return i - j + .75;}

int main()
{
    assert(std::ptr_fun(binary_f)(36, 27) == 9.75);
}
