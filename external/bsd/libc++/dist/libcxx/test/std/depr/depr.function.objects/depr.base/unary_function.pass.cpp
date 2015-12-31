//===----------------------------------------------------------------------===//
//
//                     The LLVM Compiler Infrastructure
//
// This file is dual licensed under the MIT and the University of Illinois Open
// Source Licenses. See LICENSE.TXT for details.
//
//===----------------------------------------------------------------------===//

// <functional>

// template <class Arg, class Result>
// struct unary_function
// {
//     typedef Arg    argument_type;
//     typedef Result result_type;
// };

#include <functional>
#include <type_traits>

int main()
{
    static_assert((std::is_same<std::unary_function<unsigned, char>::argument_type, unsigned>::value), "");
    static_assert((std::is_same<std::unary_function<unsigned, char>::result_type, char>::value), "");
}
