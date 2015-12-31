//===----------------------------------------------------------------------===//
//
//                     The LLVM Compiler Infrastructure
//
// This file is dual licensed under the MIT and the University of Illinois Open
// Source Licenses. See LICENSE.TXT for details.
//
//===----------------------------------------------------------------------===//

// <unordered_map>

// void swap(unordered_map& c)
//      noexcept(
//          (!allocator_type::propagate_on_container_swap::value ||
//           __is_nothrow_swappable<allocator_type>::value) &&
//           __is_nothrow_swappable<hasher>::value &&
//           __is_nothrow_swappable<key_equal>::value);
//
//  In C++17, the standard says that swap shall have:
//     noexcept(allocator_traits<Allocator>::is_always_equal::value &&
//               noexcept(swap(declval<Hash&>(), declval<Hash&>())) &&
//               noexcept(swap(declval<Pred&>(), declval<Pred&>())));

// This tests a conforming extension

#include <unordered_map>
#include <cassert>

#include "MoveOnly.h"
#include "test_allocator.h"

template <class T>
struct some_comp
{
    typedef T value_type;
    
    some_comp() {}
    some_comp(const some_comp&) {}
};

template <class T>
struct some_comp2
{
    typedef T value_type;
    
    some_comp2() {}
    some_comp2(const some_comp2&) {}
    void deallocate(void*, unsigned) {}
    typedef std::true_type propagate_on_container_swap;
};

#if TEST_STD_VER >= 14
template <typename T>
void swap(some_comp2<T>&, some_comp2<T>&) noexcept {}
#endif

template <class T>
struct some_hash
{
    typedef T value_type;
    some_hash() {}
    some_hash(const some_hash&);
};

template <class T>
struct some_hash2
{
    typedef T value_type;
    some_hash2() {}
    some_hash2(const some_hash2&);
};

#if TEST_STD_VER >= 14
template <typename T>
void swap(some_hash2<T>&, some_hash2<T>&) noexcept {}
#endif

template <class T>
struct some_alloc
{
    typedef T value_type;
    
    some_alloc() {}
    some_alloc(const some_alloc&);
    void deallocate(void*, unsigned) {}

    typedef std::true_type propagate_on_container_swap;
};

template <class T>
struct some_alloc2
{
    typedef T value_type;
    
    some_alloc2() {}
    some_alloc2(const some_alloc2&);
    void deallocate(void*, unsigned) {}

    typedef std::false_type propagate_on_container_swap;
    typedef std::true_type is_always_equal;
};

template <class T>
struct some_alloc3
{
    typedef T value_type;
    
    some_alloc3() {}
    some_alloc3(const some_alloc3&);
    void deallocate(void*, unsigned) {}

    typedef std::false_type propagate_on_container_swap;
    typedef std::false_type is_always_equal;
};


int main()
{
#if __has_feature(cxx_noexcept)
	typedef std::pair<const MoveOnly, MoveOnly> MapType;
    {
        typedef std::unordered_map<MoveOnly, MoveOnly> C;
        C c1, c2;
        static_assert(noexcept(swap(c1, c2)), "");
    }
    {
        typedef std::unordered_map<MoveOnly, MoveOnly, std::hash<MoveOnly>,
                           std::equal_to<MoveOnly>, test_allocator<MapType>> C;
        C c1, c2;
        static_assert(noexcept(swap(c1, c2)), "");
    }
    {
        typedef std::unordered_map<MoveOnly, MoveOnly, std::hash<MoveOnly>,
                          std::equal_to<MoveOnly>, other_allocator<MapType>> C;
        C c1, c2;
        static_assert(noexcept(swap(c1, c2)), "");
    }
    {
        typedef std::unordered_map<MoveOnly, MoveOnly, some_hash<MoveOnly>> C;
        C c1, c2;
        static_assert(!noexcept(swap(c1, c2)), "");
    }
    {
        typedef std::unordered_map<MoveOnly, MoveOnly, std::hash<MoveOnly>,
                                                         some_comp<MoveOnly>> C;
        C c1, c2;
        static_assert(!noexcept(swap(c1, c2)), "");
    }

#if TEST_STD_VER >= 14
    { // POCS allocator, throwable swap for hash, throwable swap for comp
    typedef std::unordered_map<MoveOnly, MoveOnly, some_hash<MoveOnly>, some_comp <MoveOnly>, some_alloc <MapType>> C;
    C c1, c2;
    static_assert(!noexcept(swap(c1, c2)), "");
    }
    { // always equal allocator, throwable swap for hash, throwable swap for comp
    typedef std::unordered_map<MoveOnly, MoveOnly, some_hash<MoveOnly>, some_comp <MoveOnly>, some_alloc2<MapType>> C;
    C c1, c2;
    static_assert(!noexcept(swap(c1, c2)), "");
    }
    { // POCS allocator, throwable swap for hash, nothrow swap for comp
    typedef std::unordered_map<MoveOnly, MoveOnly, some_hash<MoveOnly>, some_comp2<MoveOnly>, some_alloc <MapType>> C;
    C c1, c2;
    static_assert(!noexcept(swap(c1, c2)), "");
    }
    { // always equal allocator, throwable swap for hash, nothrow swap for comp
    typedef std::unordered_map<MoveOnly, MoveOnly, some_hash<MoveOnly>, some_comp2<MoveOnly>, some_alloc2<MapType>> C;
    C c1, c2;
    static_assert(!noexcept(swap(c1, c2)), "");
    }
    { // POCS allocator, nothrow swap for hash, throwable swap for comp
    typedef std::unordered_map<MoveOnly, MoveOnly, some_hash2<MoveOnly>, some_comp <MoveOnly>, some_alloc <MapType>> C;
    C c1, c2;
    static_assert(!noexcept(swap(c1, c2)), "");
    }
    { // always equal allocator, nothrow swap for hash, throwable swap for comp
    typedef std::unordered_map<MoveOnly, MoveOnly, some_hash2<MoveOnly>, some_comp <MoveOnly>, some_alloc2<MapType>> C;
    C c1, c2;
    static_assert(!noexcept(swap(c1, c2)), "");
    }
    { // POCS allocator, nothrow swap for hash, nothrow swap for comp
    typedef std::unordered_map<MoveOnly, MoveOnly, some_hash2<MoveOnly>, some_comp2<MoveOnly>, some_alloc <MapType>> C;
    C c1, c2;
    static_assert( noexcept(swap(c1, c2)), "");
    }
    { // always equal allocator, nothrow swap for hash, nothrow swap for comp
    typedef std::unordered_map<MoveOnly, MoveOnly, some_hash2<MoveOnly>, some_comp2<MoveOnly>, some_alloc2<MapType>> C;
    C c1, c2;
    static_assert( noexcept(swap(c1, c2)), "");
    }

    { // NOT always equal allocator, nothrow swap for hash, nothrow swap for comp
    typedef std::unordered_map<MoveOnly, MoveOnly, some_hash2<MoveOnly>, some_comp2<MoveOnly>, some_alloc3<MapType>> C;
    C c1, c2;
    static_assert( noexcept(swap(c1, c2)), "");
    }
#endif
#endif
}
