//===----------------------------------------------------------------------===//
//
//                     The LLVM Compiler Infrastructure
//
// This file is dual licensed under the MIT and the University of Illinois Open
// Source Licenses. See LICENSE.TXT for details.
//
//===----------------------------------------------------------------------===//

// <string>

// ~basic_string() // implied noexcept;

#include <string>
#include <cassert>

#include "test_allocator.h"

#if __has_feature(cxx_noexcept)

template <class T>
struct some_alloc
{
    typedef T value_type;
    some_alloc(const some_alloc&);
    ~some_alloc() noexcept(false);
};

#endif

int main()
{
#if __has_feature(cxx_noexcept)
    {
        typedef std::string C;
        static_assert(std::is_nothrow_destructible<C>::value, "");
    }
    {
        typedef std::basic_string<char, std::char_traits<char>, test_allocator<char>> C;
        static_assert(std::is_nothrow_destructible<C>::value, "");
    }
    {
        typedef std::basic_string<char, std::char_traits<char>, some_alloc<char>> C;
        static_assert(!std::is_nothrow_destructible<C>::value, "");
    }
#endif
}
