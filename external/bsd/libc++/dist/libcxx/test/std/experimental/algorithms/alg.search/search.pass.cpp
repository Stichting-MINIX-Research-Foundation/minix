//===----------------------------------------------------------------------===//
//
//                     The LLVM Compiler Infrastructure
//
// This file is dual licensed under the MIT and the University of Illinois Open
// Source Licenses. See LICENSE.TXT for details.
//
//===----------------------------------------------------------------------===//

// <algorithm>

//   template<class ForwardIterator, class Searcher>
//   ForwardIterator search(ForwardIterator first, ForwardIterator last,
//                          const Searcher& searcher);
//
//		returns searcher.operator(first, last)
//

#include <experimental/algorithm>
#include <cassert>

#include "test_iterators.h"

int searcher_called = 0;

struct MySearcher {
    template <typename Iterator>
    Iterator operator() ( Iterator b, Iterator /*e*/) const 
    {
        ++searcher_called;
        return b;
    }
};


int main() {
    typedef int * RI;
    static_assert(std::is_same<RI, decltype(std::experimental::search(RI(), RI(), MySearcher()))>::value, "" );

    RI it{nullptr};
    assert(it == std::experimental::search(it, it, MySearcher()));
    assert(searcher_called == 1);
}
