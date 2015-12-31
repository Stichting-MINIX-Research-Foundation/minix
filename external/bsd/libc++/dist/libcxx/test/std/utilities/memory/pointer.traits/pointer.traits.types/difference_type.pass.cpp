//===----------------------------------------------------------------------===//
//
//                     The LLVM Compiler Infrastructure
//
// This file is dual licensed under the MIT and the University of Illinois Open
// Source Licenses. See LICENSE.TXT for details.
//
//===----------------------------------------------------------------------===//

// <memory>

// template <class Ptr>
// struct pointer_traits
// {
//     typedef <details> difference_type;
//     ...
// };

#include <memory>
#include <type_traits>

struct A
{
    typedef short element_type;
    typedef char difference_type;
};

struct B
{
    typedef short element_type;
};

template <class T>
struct C {};

template <class T>
struct D
{
    typedef char difference_type;
};

int main()
{
    static_assert((std::is_same<std::pointer_traits<A>::difference_type, char>::value), "");
    static_assert((std::is_same<std::pointer_traits<B>::difference_type, std::ptrdiff_t>::value), "");
    static_assert((std::is_same<std::pointer_traits<C<double> >::difference_type, std::ptrdiff_t>::value), "");
    static_assert((std::is_same<std::pointer_traits<D<int> >::difference_type, char>::value), "");
}
