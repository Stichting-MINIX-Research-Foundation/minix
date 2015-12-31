//===----------------------------------------------------------------------===//
//
//                     The LLVM Compiler Infrastructure
//
// This file is dual licensed under the MIT and the University of Illinois Open
// Source Licenses. See LICENSE.TXT for details.
//
//===----------------------------------------------------------------------===//

// <optional>

// template <class T>
// class optional
// {
// public:
//     typedef T value_type;
//     ...

#include <experimental/optional>
#include <type_traits>

#if _LIBCPP_STD_VER > 11

using std::experimental::optional;

template <class Opt, class T>
void
test()
{
    static_assert(std::is_same<typename Opt::value_type, T>::value, "");
}

#endif  // _LIBCPP_STD_VER > 11

int main()
{
#if _LIBCPP_STD_VER > 11
    test<optional<int>, int>();
    test<optional<const int>, const int>();
    test<optional<double>, double>();
    test<optional<const double>, const double>();
#endif  // _LIBCPP_STD_VER > 11
}
