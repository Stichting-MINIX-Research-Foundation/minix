//===----------------------------------------------------------------------===//
//
//                     The LLVM Compiler Infrastructure
//
// This file is dual licensed under the MIT and the University of Illinois Open
// Source Licenses. See LICENSE.TXT for details.
//
//===----------------------------------------------------------------------===//

// <iterator>

// move_iterator

// reference operator*() const;

#include <iterator>
#include <cassert>
#ifndef _LIBCPP_HAS_NO_RVALUE_REFERENCES
#include <memory>
#endif

class A
{
    int data_;
public:
    A() : data_(1) {}
    ~A() {data_ = -1;}

    friend bool operator==(const A& x, const A& y)
        {return x.data_ == y.data_;}
};

template <class It>
void
test(It i, typename std::iterator_traits<It>::value_type x)
{
    std::move_iterator<It> r(i);
    assert(*r == x);
    typename std::iterator_traits<It>::value_type x2 = *r;
    assert(x2 == x);
}

#ifndef _LIBCPP_HAS_NO_RVALUE_REFERENCES

struct do_nothing
{
    void operator()(void*) const {}
};

#endif  // _LIBCPP_HAS_NO_RVALUE_REFERENCES

int main()
{
    A a;
    test(&a, A());
#ifndef _LIBCPP_HAS_NO_RVALUE_REFERENCES
    int i;
    std::unique_ptr<int, do_nothing> p(&i);
    test(&p, std::unique_ptr<int, do_nothing>(&i));
#endif  // _LIBCPP_HAS_NO_RVALUE_REFERENCES
}
