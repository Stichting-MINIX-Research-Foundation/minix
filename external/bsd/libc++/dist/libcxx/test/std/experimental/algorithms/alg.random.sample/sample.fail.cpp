//===----------------------------------------------------------------------===//
//
//                     The LLVM Compiler Infrastructure
//
// This file is dual licensed under the MIT and the University of Illinois Open
// Source Licenses. See LICENSE.TXT for details.
//
//===----------------------------------------------------------------------===//

// <algorithm>

// template <class PopulationIterator, class SampleIterator, class Distance,
//           class UniformRandomNumberGenerator>
// SampleIterator sample(PopulationIterator first, PopulationIterator last,
//                       SampleIterator out, Distance n,
//                       UniformRandomNumberGenerator &&g);

#include <experimental/algorithm>
#include <random>
#include <cassert>

#include "test_iterators.h"

template <class PopulationIterator, class SampleIterator> void test() {
  int ia[] = {1, 2, 3, 4, 5, 6, 7, 8, 9, 10};
  const unsigned is = sizeof(ia) / sizeof(ia[0]);
  const unsigned os = 4;
  int oa[os];
  std::minstd_rand g;
  std::experimental::sample(PopulationIterator(ia), PopulationIterator(ia + is),
                            SampleIterator(oa), os, g);
}

int main() {
  test<input_iterator<int *>, output_iterator<int *> >();
}
