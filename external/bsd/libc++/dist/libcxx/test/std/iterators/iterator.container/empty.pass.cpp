//===----------------------------------------------------------------------===//
//
//                     The LLVM Compiler Infrastructure
//
// This file is dual licensed under the MIT and the University of Illinois Open
// Source Licenses. See LICENSE.TXT for details.
//
//===----------------------------------------------------------------------===//

// <iterator>
// template <class C> constexpr auto empty(const C& c) -> decltype(c.empty());       // C++17
// template <class T, size_t N> constexpr bool empty(const T (&array)[N]) noexcept;  // C++17
// template <class E> constexpr bool empty(initializer_list<E> il) noexcept;         // C++17

#if __cplusplus <= 201402L
int main () {}
#else

#include <iterator>
#include <cassert>
#include <vector>
#include <array>
#include <list>
#include <initializer_list>

template<typename C>
void test_const_container( const C& c )
{
    assert ( std::empty(c)   == c.empty());
}

template<typename T>
void test_const_container( const std::initializer_list<T>& c )
{
    assert ( std::empty(c)   == (c.size() == 0));
}

template<typename C>
void test_container( C& c )
{
    assert ( std::empty(c)   == c.empty());
}
    
template<typename T>
void test_container( std::initializer_list<T>& c )
{
    assert ( std::empty(c)   == (c.size() == 0));
}

template<typename T, size_t Sz>
void test_const_array( const T (&array)[Sz] )
{
    assert (!std::empty(array));
}

int main()
{
    std::vector<int> v; v.push_back(1);
    std::list<int>   l; l.push_back(2);
    std::array<int, 1> a; a[0] = 3;
    std::initializer_list<int> il = { 4 };
    
    test_container ( v );
    test_container ( l );
    test_container ( a );
    test_container ( il );

    test_const_container ( v );
    test_const_container ( l );
    test_const_container ( a );
    test_const_container ( il );
    
    static constexpr int arrA [] { 1, 2, 3 };
    test_const_array ( arrA );
}

#endif
