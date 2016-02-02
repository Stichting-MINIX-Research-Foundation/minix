//===----------------------------------------------------------------------===//
//
//                     The LLVM Compiler Infrastructure
//
// This file is dual licensed under the MIT and the University of Illinois Open
// Source Licenses. See LICENSE.TXT for details.
//
//===----------------------------------------------------------------------===//

// <optional>

// constexpr optional(const T& v);

#include <experimental/optional>
#include <type_traits>
#include <cassert>

#if _LIBCPP_STD_VER > 11

using std::experimental::optional;

class X
{
    int i_;
public:
    X(int i) : i_(i) {}

    friend bool operator==(const X& x, const X& y) {return x.i_ == y.i_;}
};

class Y
{
    int i_;
public:
    constexpr Y(int i) : i_(i) {}

    friend constexpr bool operator==(const Y& x, const Y& y) {return x.i_ == y.i_;}
};

class Z
{
    int i_;
public:
    Z(int i) : i_(i) {}
    Z(const Z&) {throw 6;}
};


#endif  // _LIBCPP_STD_VER > 11

int main()
{
#if _LIBCPP_STD_VER > 11
    {
        typedef int T;
        constexpr T t(5);
        constexpr optional<T> opt(t);
        static_assert(static_cast<bool>(opt) == true, "");
        static_assert(*opt == 5, "");

        struct test_constexpr_ctor
            : public optional<T>
        {
            constexpr test_constexpr_ctor(const T&) {}
        };

    }
    {
        typedef double T;
        constexpr T t(3);
        constexpr optional<T> opt(t);
        static_assert(static_cast<bool>(opt) == true, "");
        static_assert(*opt == 3, "");

        struct test_constexpr_ctor
            : public optional<T>
        {
            constexpr test_constexpr_ctor(const T&) {}
        };

    }
    {
        typedef X T;
        const T t(3);
        optional<T> opt(t);
        assert(static_cast<bool>(opt) == true);
        assert(*opt == 3);
    }
    {
        typedef Y T;
        constexpr T t(3);
        constexpr optional<T> opt(t);
        static_assert(static_cast<bool>(opt) == true, "");
        static_assert(*opt == 3, "");

        struct test_constexpr_ctor
            : public optional<T>
        {
            constexpr test_constexpr_ctor(const T&) {}
        };

    }
    {
        typedef Z T;
        try
        {
            const T t(3);
            optional<T> opt(t);
            assert(false);
        }
        catch (int i)
        {
            assert(i == 6);
        }
    }
#endif  // _LIBCPP_STD_VER > 11
}
