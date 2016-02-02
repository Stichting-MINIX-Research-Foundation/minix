//===----------------------------------------------------------------------===//
//
//                     The LLVM Compiler Infrastructure
//
// This file is dual licensed under the MIT and the University of Illinois Open
// Source Licenses. See LICENSE.TXT for details.
//
//===----------------------------------------------------------------------===//

// test bad_array_length

#include <new>
#include <type_traits>
#include <cassert>

int main()
{
#if __LIBCPP_STD_VER > 11
    static_assert((std::is_base_of<std::bad_alloc, std::bad_array_length>::value),
                  "std::is_base_of<std::bad_alloc, std::bad_array_length>::value");
    static_assert(std::is_polymorphic<std::bad_array_length>::value,
                 "std::is_polymorphic<std::bad_array_length>::value");
    std::bad_array_length b;
    std::bad_array_length b2 = b;
    b2 = b;
    const char* w = b2.what();
    assert(w);
#endif
}
