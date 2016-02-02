//===----------------------------------------------------------------------===//
//
//                     The LLVM Compiler Infrastructure
//
// This file is dual licensed under the MIT and the University of Illinois Open
// Source Licenses. See LICENSE.TXT for details.
//
//===----------------------------------------------------------------------===//

// <algorithm>

// template<BidirectionalIterator Iter, StrictWeakOrder<auto, Iter::value_type> Compare>
//   requires ShuffleIterator<Iter>
//         && CopyConstructible<Compare>
//   void
//   inplace_merge(Iter first, Iter middle, Iter last, Compare comp);

#include <algorithm>
#include <functional>
#include <cassert>

#include "test_macros.h"

#if TEST_STD_VER >= 11
#include <memory>

struct indirect_less
{
    template <class P>
    bool operator()(const P& x, const P& y)
        {return *x < *y;}
};

struct S {
	S() : i_(0) {}
	S(int i) : i_(i) {}
	
	S(const S&  rhs) : i_(rhs.i_) {}
	S(      S&& rhs) : i_(rhs.i_) { rhs.i_ = -1; }
	
	S& operator =(const S&  rhs) { i_ = rhs.i_;              return *this; }
	S& operator =(      S&& rhs) { i_ = rhs.i_; rhs.i_ = -2; assert(this != &rhs); return *this; }
	S& operator =(int i)         { i_ = i;                   return *this; }
	
	bool operator  <(const S&  rhs) const { return i_ < rhs.i_; }
	bool operator  >(const S&  rhs) const { return i_ > rhs.i_; }
	bool operator ==(const S&  rhs) const { return i_ == rhs.i_; }
	bool operator ==(int i)         const { return i_ == i; }

	void set(int i) { i_ = i; }
	
	int i_;
	};


#endif  // TEST_STD_VER >= 11

#include "test_iterators.h"
#include "counting_predicates.hpp"

template <class Iter>
void
test_one(unsigned N, unsigned M)
{
    assert(M <= N);
    typedef typename std::iterator_traits<Iter>::value_type value_type;
    value_type* ia = new value_type[N];
    for (unsigned i = 0; i < N; ++i)
        ia[i] = i;
    std::random_shuffle(ia, ia+N);
    std::sort(ia, ia+M, std::greater<value_type>());
    std::sort(ia+M, ia+N, std::greater<value_type>());
    binary_counting_predicate<std::greater<value_type>, value_type, value_type> pred((std::greater<value_type>()));
    std::inplace_merge(Iter(ia), Iter(ia+M), Iter(ia+N), std::ref(pred));
    if(N > 0)
    {
        assert(ia[0] == N-1);
        assert(ia[N-1] == 0);
        assert(std::is_sorted(ia, ia+N, std::greater<value_type>()));
        assert(pred.count() <= (N-1));
    }
    delete [] ia;
}

template <class Iter>
void
test(unsigned N)
{
    test_one<Iter>(N, 0);
    test_one<Iter>(N, N/4);
    test_one<Iter>(N, N/2);
    test_one<Iter>(N, 3*N/4);
    test_one<Iter>(N, N);
}

template <class Iter>
void
test()
{
    test_one<Iter>(0, 0);
    test_one<Iter>(1, 0);
    test_one<Iter>(1, 1);
    test_one<Iter>(2, 0);
    test_one<Iter>(2, 1);
    test_one<Iter>(2, 2);
    test_one<Iter>(3, 0);
    test_one<Iter>(3, 1);
    test_one<Iter>(3, 2);
    test_one<Iter>(3, 3);
    test<Iter>(4);
    test<Iter>(20);
    test<Iter>(100);
    test<Iter>(1000);
}

int main()
{
    test<bidirectional_iterator<int*> >();
    test<random_access_iterator<int*> >();
    test<int*>();

#if TEST_STD_VER >= 11
    test<bidirectional_iterator<S*> >();
    test<random_access_iterator<S*> >();
    test<S*>();

    {
    unsigned N = 100;
    unsigned M = 50;
    std::unique_ptr<int>* ia = new std::unique_ptr<int>[N];
    for (unsigned i = 0; i < N; ++i)
        ia[i].reset(new int(i));
    std::random_shuffle(ia, ia+N);
    std::sort(ia, ia+M, indirect_less());
    std::sort(ia+M, ia+N, indirect_less());
    std::inplace_merge(ia, ia+M, ia+N, indirect_less());
    if(N > 0)
    {
        assert(*ia[0] == 0);
        assert(*ia[N-1] == N-1);
        assert(std::is_sorted(ia, ia+N, indirect_less()));
    }
    delete [] ia;
    }
#endif  // TEST_STD_VER >= 11
}
