//===----------------------------------------------------------------------===//
//
//                     The LLVM Compiler Infrastructure
//
// This file is dual licensed under the MIT and the University of Illinois Open
// Source Licenses. See LICENSE.TXT for details.
//
//===----------------------------------------------------------------------===//

#include <cstddef>
#include <type_traits>

// max_align_t is a POD type whose alignment requirement is at least as
//   great as that of every scalar type

#include <stdio.h>

int main()
{
    static_assert(std::is_pod<std::max_align_t>::value,
                  "std::is_pod<std::max_align_t>::value");
    static_assert((std::alignment_of<std::max_align_t>::value >=
                  std::alignment_of<long long>::value),
                  "std::alignment_of<std::max_align_t>::value >= "
                  "std::alignment_of<long long>::value");
    static_assert(std::alignment_of<std::max_align_t>::value >=
                  std::alignment_of<long double>::value,
                  "std::alignment_of<std::max_align_t>::value >= "
                  "std::alignment_of<long double>::value");
    static_assert(std::alignment_of<std::max_align_t>::value >=
                  std::alignment_of<void*>::value,
                  "std::alignment_of<std::max_align_t>::value >= "
                  "std::alignment_of<void*>::value");
}
