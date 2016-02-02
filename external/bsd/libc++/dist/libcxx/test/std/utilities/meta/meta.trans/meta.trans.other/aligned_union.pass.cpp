//===----------------------------------------------------------------------===//
//
//                     The LLVM Compiler Infrastructure
//
// This file is dual licensed under the MIT and the University of Illinois Open
// Source Licenses. See LICENSE.TXT for details.
//
//===----------------------------------------------------------------------===//

// type_traits

// aligned_union<size_t Len, class ...Types>

#include <type_traits>

int main()
{
#ifndef _LIBCPP_HAS_NO_VARIADICS
    {
    typedef std::aligned_union<10, char >::type T1;
#if _LIBCPP_STD_VER > 11
    static_assert(std::is_same<std::aligned_union_t<10, char>, T1>::value, "" );
#endif
    static_assert(std::alignment_of<T1>::value == 1, "");
    static_assert(sizeof(T1) == 10, "");
    }
    {
    typedef std::aligned_union<10, short >::type T1;
#if _LIBCPP_STD_VER > 11
    static_assert(std::is_same<std::aligned_union_t<10, short>, T1>::value, "" );
#endif
    static_assert(std::alignment_of<T1>::value == 2, "");
    static_assert(sizeof(T1) == 10, "");
    }
    {
    typedef std::aligned_union<10, int >::type T1;
#if _LIBCPP_STD_VER > 11
    static_assert(std::is_same<std::aligned_union_t<10, int>, T1>::value, "" );
#endif
    static_assert(std::alignment_of<T1>::value == 4, "");
    static_assert(sizeof(T1) == 12, "");
    }
    {
    typedef std::aligned_union<10, double >::type T1;
#if _LIBCPP_STD_VER > 11
    static_assert(std::is_same<std::aligned_union_t<10, double>, T1>::value, "" );
#endif
    static_assert(std::alignment_of<T1>::value == 8, "");
    static_assert(sizeof(T1) == 16, "");
    }
    {
    typedef std::aligned_union<10, short, char >::type T1;
#if _LIBCPP_STD_VER > 11
    static_assert(std::is_same<std::aligned_union_t<10, short, char>, T1>::value, "" );
#endif
    static_assert(std::alignment_of<T1>::value == 2, "");
    static_assert(sizeof(T1) == 10, "");
    }
    {
    typedef std::aligned_union<10, char, short >::type T1;
#if _LIBCPP_STD_VER > 11
    static_assert(std::is_same<std::aligned_union_t<10, char, short>, T1>::value, "" );
#endif
    static_assert(std::alignment_of<T1>::value == 2, "");
    static_assert(sizeof(T1) == 10, "");
    }
    {
    typedef std::aligned_union<2, int, char, short >::type T1;
#if _LIBCPP_STD_VER > 11
    static_assert(std::is_same<std::aligned_union_t<2, int, char, short>, T1>::value, "" );
#endif
    static_assert(std::alignment_of<T1>::value == 4, "");
    static_assert(sizeof(T1) == 4, "");
    }
    {
    typedef std::aligned_union<2, char, int, short >::type T1;
#if _LIBCPP_STD_VER > 11
    static_assert(std::is_same<std::aligned_union_t<2, char, int, short >, T1>::value, "" );
#endif
    static_assert(std::alignment_of<T1>::value == 4, "");
    static_assert(sizeof(T1) == 4, "");
    }
    {
    typedef std::aligned_union<2, char, short, int >::type T1;
#if _LIBCPP_STD_VER > 11
    static_assert(std::is_same<std::aligned_union_t<2, char, short, int >, T1>::value, "" );
#endif
    static_assert(std::alignment_of<T1>::value == 4, "");
    static_assert(sizeof(T1) == 4, "");
    }
#endif
}
