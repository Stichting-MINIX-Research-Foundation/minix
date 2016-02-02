//===----------------------------------------------------------------------===//
//
//                     The LLVM Compiler Infrastructure
//
// This file is dual licensed under the MIT and the University of Illinois Open
// Source Licenses. See LICENSE.TXT for details.
//
//===----------------------------------------------------------------------===//

// <functional>

// reference_wrapper

// Test that reference wrapper meets the requirements of TriviallyCopyable,
// CopyConstructible and CopyAssignable.

#include <functional>
#include <type_traits>
#include <string>

#ifndef _LIBCPP_HAS_NO_RVALUE_REFERENCES
class MoveOnly
{
    MoveOnly(const MoveOnly&);
    MoveOnly& operator=(const MoveOnly&);

    int data_;
public:
    MoveOnly(int data = 1) : data_(data) {}
    MoveOnly(MoveOnly&& x)
        : data_(x.data_) {x.data_ = 0;}
    MoveOnly& operator=(MoveOnly&& x)
        {data_ = x.data_; x.data_ = 0; return *this;}

    int get() const {return data_;}
};
#endif


template <class T>
void test()
{
    typedef std::reference_wrapper<T> Wrap;
    static_assert(std::is_copy_constructible<Wrap>::value, "");
    static_assert(std::is_copy_assignable<Wrap>::value, "");
    // Extension up for standardization: See N4151.
    static_assert(std::is_trivially_copyable<Wrap>::value, "");
}

int main()
{
    test<int>();
    test<double>();
    test<std::string>(); 
#ifndef _LIBCPP_HAS_NO_RVALUE_REFERENCES
    test<MoveOnly>(); 
#endif
}
