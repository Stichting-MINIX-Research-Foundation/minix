//===----------------------------------------------------------------------===//
//
//                     The LLVM Compiler Infrastructure
//
// This file is dual licensed under the MIT and the University of Illinois Open
// Source Licenses. See LICENSE.TXT for details.
//
//===----------------------------------------------------------------------===//

// <optional>

// optional(const optional<T>& rhs);

#include <experimental/optional>
#include <type_traits>
#include <cassert>

#if _LIBCPP_STD_VER > 11

using std::experimental::optional;

template <class T>
void
test(const optional<T>& rhs, bool is_going_to_throw = false)
{
    bool rhs_engaged = static_cast<bool>(rhs);
    try
    {
        optional<T> lhs = rhs;
        assert(is_going_to_throw == false);
        assert(static_cast<bool>(lhs) == rhs_engaged);
        if (rhs_engaged)
            assert(*lhs == *rhs);
    }
    catch (int i)
    {
        assert(i == 6);
    }
}

class X
{
    int i_;
public:
    X(int i) : i_(i) {}
    X(const X& x) : i_(x.i_) {}
    ~X() {i_ = 0;}
    friend bool operator==(const X& x, const X& y) {return x.i_ == y.i_;}
};

class Y
{
    int i_;
public:
    Y(int i) : i_(i) {}
    Y(const Y& x) : i_(x.i_) {}

    friend constexpr bool operator==(const Y& x, const Y& y) {return x.i_ == y.i_;}
};

int count = 0;

class Z
{
    int i_;
public:
    Z(int i) : i_(i) {}
    Z(const Z&)
    {
        if (++count == 2)
            throw 6;
    }

    friend constexpr bool operator==(const Z& x, const Z& y) {return x.i_ == y.i_;}
};


#endif  // _LIBCPP_STD_VER > 11

int main()
{
#if _LIBCPP_STD_VER > 11
    {
        typedef int T;
        optional<T> rhs;
        test(rhs);
    }
    {
        typedef int T;
        optional<T> rhs(3);
        test(rhs);
    }
    {
        typedef X T;
        optional<T> rhs;
        test(rhs);
    }
    {
        typedef X T;
        optional<T> rhs(X(3));
        test(rhs);
    }
    {
        typedef Y T;
        optional<T> rhs;
        test(rhs);
    }
    {
        typedef Y T;
        optional<T> rhs(Y(3));
        test(rhs);
    }
    {
        typedef Z T;
        optional<T> rhs;
        test(rhs);
    }
    {
        typedef Z T;
        optional<T> rhs(Z(3));
        test(rhs, true);
    }
#endif  // _LIBCPP_STD_VER > 11
}
